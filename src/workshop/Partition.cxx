/*
 * Copyright 2006-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Partition.hxx"
#include "Instance.hxx"
#include "MultiLibrary.hxx"
#include "Config.hxx"
#include "Job.hxx"
#include "Plan.hxx"
#include "../Config.hxx"
#include "pg/Array.hxx"
#include "util/Exception.hxx"

#include <set>

WorkshopPartition::WorkshopPartition(Instance &_instance,
				     MultiLibrary &_library,
				     SpawnService &_spawn_service,
				     const Config &root_config,
				     const WorkshopPartitionConfig &config,
				     BoundMethod<void() noexcept> _idle_callback) noexcept
	:logger("workshop"), // TODO: add partition name
	 instance(_instance), library(_library),
	 rate_limit_timer(instance.GetEventLoop(),
			  BIND_THIS_METHOD(OnRateLimitTimer)),
	 reap_timer(instance.GetEventLoop(), BIND_THIS_METHOD(OnReapTimer)),
	 queue(logger, instance.GetEventLoop(), root_config.node_name.c_str(),
	       config.database.c_str(), config.database_schema.c_str(),
	       *this),
	 workplace(_spawn_service, *this, logger,
		   root_config.node_name.c_str(),
		   config.translation_socket,
		   config.tag.empty() ? nullptr : config.tag.c_str(),
		   root_config.concurrency,
		   config.enable_journal),
	 idle_callback(_idle_callback),
	 max_log(config.max_log)
{
	ScheduleReapFinished();
}

void
WorkshopPartition::OnRateLimitTimer() noexcept
{
	UpdateFilter();
}

void
WorkshopPartition::UpdateFilter(bool library_modified)
{
	std::set<std::string_view, std::less<>> available_plans;
	library.VisitAvailable(GetEventLoop().SteadyNow(),
			       [&available_plans](const std::string_view name, const Plan &){
				       available_plans.emplace(name);
			       });

	if (library_modified)
		rate_limited_plans.clear();

	/* remove the plans which have hit their rate limit */
	const auto now = GetEventLoop().SteadyNow();
	const auto earliest_expiry =
		rate_limited_plans.ForEach(now,
			[&available_plans](const std::string_view name){
				auto i = available_plans.find(name);
				if (i != available_plans.end())
					available_plans.erase(i);
			});
	if (earliest_expiry.IsExpired(now))
		rate_limit_timer.Cancel();
	else
		rate_limit_timer.Schedule(earliest_expiry.GetRemainingDuration(now));

	queue.SetFilter(Pg::EncodeArray(available_plans),
			workplace.GetFullPlanNames(),
			workplace.GetRunningPlanNames());

	if (library_modified)
		ScheduleReapFinished();
}

void
WorkshopPartition::UpdateLibraryAndFilter(bool force)
{
	instance.UpdateLibraryAndFilter(force);
}

bool
WorkshopPartition::StartJob(WorkshopJob &&job,
			    std::shared_ptr<Plan> plan)
{
	try {
		workplace.Start(instance.GetEventLoop(), job, std::move(plan),
				max_log);
	} catch (...) {
		logger(1, "failed to start job '", job.id,
		       "' plan '", job.plan_name, "': ", std::current_exception());

		queue.SetJobDone(job, -1, nullptr);
	}

	return true;
}

void
WorkshopPartition::OnReapTimer() noexcept
{
	logger(6, "Reaping finished jobs");

	bool found = false;

	library.VisitAvailable(GetEventLoop().SteadyNow(), [this, &found](const std::string &name, const Plan &plan){
		if (plan.reap_finished.empty())
			return;

		unsigned n = queue.ReapFinishedJobs(name.c_str(),
						    plan.reap_finished.c_str());
		if (n > 0) {
			found = true;
			logger(5, "Reaped ", n, " jobs of plan '",
			       name.c_str(), "'");
		}
	});

	if (found)
		/* keep on watching for jobs to be reaped as long as
		   we're busy */
		ScheduleReapFinished();
}

void
WorkshopPartition::ScheduleReapFinished() noexcept
{
	if (reap_timer.IsPending())
		return;

	// TODO: randomize?  per-plan timer?
	reap_timer.Schedule(std::chrono::seconds(10));
}

std::shared_ptr<Plan>
WorkshopPartition::GetWorkshopPlan(const char *name) noexcept
{
	return library.Get(GetEventLoop().SteadyNow(), name);
}

inline std::chrono::seconds
WorkshopPartition::CheckRateLimit(const char *plan_name,
				  const Plan &plan) noexcept
{
	for (const auto &rate_limit : plan.rate_limits) {
		assert(rate_limit.IsDefined());

		auto delta = queue.CheckRateLimit(plan_name,
						  rate_limit.duration,
						  rate_limit.max_count);
		if (delta > std::chrono::seconds{})
			return delta;
	}

	return {};
}

bool
WorkshopPartition::CheckWorkshopJob(const WorkshopJob &job,
				    const Plan &plan) noexcept
{
	if (workplace.IsFull()) {
		queue.DisableFull();
		return false;
	}

	auto delta = CheckRateLimit(job.plan_name.c_str(), plan);
	if (delta > std::chrono::seconds{}) {
		logger(4, "Rate limit of '", job.plan_name, "' hit");

		rate_limited_plans.Set(job.plan_name,
				       Expiry::Touched(GetEventLoop().SteadyNow(),
						       delta));

		UpdateFilter();
		return false;
	}

	return true;
}

void
WorkshopPartition::StartWorkshopJob(WorkshopJob &&job,
				    std::shared_ptr<Plan> plan) noexcept
{
	if (!StartJob(std::move(job), std::move(plan)) || workplace.IsFull())
		queue.DisableFull();

	UpdateFilter();
}

void
WorkshopPartition::OnChildProcessExit(int) noexcept
{
	ScheduleReapFinished();

	UpdateLibraryAndFilter(false);

	if (!workplace.IsFull())
		queue.EnableFull();

	if (IsIdle())
		idle_callback();
}
