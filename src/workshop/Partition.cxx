/*
 * Copyright 2006-2018 Content Management AG
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

/* TODO: find out when the rate limit expires and use that time stamp
   instead of hard-coding this value */
static constexpr auto RATE_LIMIT_TIMER_INTERVAL = std::chrono::minutes(1);

WorkshopPartition::WorkshopPartition(Instance &_instance,
				     MultiLibrary &_library,
				     SpawnService &_spawn_service,
				     const Config &root_config,
				     const WorkshopPartitionConfig &config,
				     BoundMethod<void()> _idle_callback)
	:logger("workshop"), // TODO: add partition name
	 instance(_instance), library(_library),
	 rate_limit_timer(instance.GetEventLoop(),
			  BIND_THIS_METHOD(OnRateLimitTimer)),
	 queue(logger, instance.GetEventLoop(), root_config.node_name.c_str(),
	       config.database.c_str(), config.database_schema.c_str(),
	       *this),
	 workplace(_spawn_service, *this, logger,
		   root_config.node_name.c_str(),
		   root_config.concurrency,
		   config.enable_journal),
	 idle_callback(_idle_callback),
	 max_log(config.max_log)
{
}

void
WorkshopPartition::OnRateLimitTimer() noexcept
{
	UpdateFilter();
}

void
WorkshopPartition::UpdateFilter()
{
	std::set<std::string> available_plans;
	library.VisitPlans(GetEventLoop().SteadyNow(),
			   [&available_plans](const std::string &name, const Plan &){
				   available_plans.emplace(name);
			   });

	/* remove the plans which have hit their rate limit */
	rate_limited_plans.ForEach(GetEventLoop().SteadyNow(),
				   [&available_plans](const std::string &name){
					   auto i = available_plans.find(name);
					   if (i != available_plans.end())
						   available_plans.erase(i);
				   });

	if (rate_limited_plans.empty())
		rate_limit_timer.Cancel();
	else
		rate_limit_timer.Schedule(RATE_LIMIT_TIMER_INTERVAL);

	queue.SetFilter(Pg::EncodeArray(available_plans),
			workplace.GetFullPlanNames(),
			workplace.GetRunningPlanNames());
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

std::shared_ptr<Plan>
WorkshopPartition::GetWorkshopPlan(const char *name) noexcept
{
	return library.Get(GetEventLoop().SteadyNow(), name);
}

bool
WorkshopPartition::CheckWorkshopJob(const WorkshopJob &job,
				    const Plan &plan) noexcept
{
	if (workplace.IsFull()) {
		queue.DisableFull();
		return false;
	}

	/* check the rate limit */
	for (const auto &rate_limit : plan.rate_limits) {
		assert(rate_limit.IsDefined());

		if (queue.CountRecentlyStartedJobs(job.plan_name.c_str(),
						   rate_limit.duration) < rate_limit.max_count)
			continue;

		logger(4, "Rate limit of '", job.plan_name, "' hit");

		rate_limited_plans.Set(job.plan_name,
				       Expiry::Touched(GetEventLoop().SteadyNow(),
						       RATE_LIMIT_TIMER_INTERVAL));

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
WorkshopPartition::OnChildProcessExit(int)
{
	UpdateLibraryAndFilter(false);

	if (!workplace.IsFull())
		queue.EnableFull();

	if (IsIdle())
		idle_callback();
}
