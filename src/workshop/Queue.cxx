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

#include "Queue.hxx"
#include "PGQueue.hxx"
#include "Job.hxx"
#include "Plan.hxx"
#include "pg/Array.hxx"
#include "pg/Reflection.hxx"
#include "event/Loop.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringCompare.hxx"

#include <stdexcept>

#include <sys/types.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

WorkshopQueue::WorkshopQueue(const Logger &parent_logger,
			     EventLoop &event_loop,
			     const char *_node_name,
			     const char *conninfo, const char *schema,
			     WorkshopQueueHandler &_handler) noexcept
	:logger(parent_logger, "queue"), node_name(_node_name),
	 db(event_loop, conninfo, schema, *this),
	 check_notify_event(event_loop, BIND_THIS_METHOD(CheckNotify)),
	 timer_event(event_loop, BIND_THIS_METHOD(OnTimer)),
	 handler(_handler)
{
}

WorkshopQueue::~WorkshopQueue() noexcept
{
	assert(!running);

	Close();
}

void
WorkshopQueue::Close() noexcept
{
	assert(!running);

	db.Disconnect();

	timer_event.Cancel();
	check_notify_event.Cancel();
}

void
WorkshopQueue::OnTimer() noexcept
{
	Run();
}

bool
WorkshopQueue::GetNextScheduled(int *span_r)
{
	long span;

	if (plans_include.empty()) {
		*span_r = -1;
		return false;
	}

	if (!pg_next_scheduled_job(db,
				   plans_include.c_str(),
				   &span)) {
		*span_r = -1;
		return false;
	}

	if (span < 0)
		span = 0;

	/* try to avoid rounding errors: always add 2 seconds */
	span += 2;

	if (span > 600)
		span = 600;

	*span_r = (int)span;
	return true;
}

static WorkshopJob
MakeJob(WorkshopQueue &queue,
	const Pg::Result &result, unsigned row)
{
	assert(row < result.GetRowCount());

	WorkshopJob job(queue);
	job.id = result.GetValue(row, 0);
	job.plan_name = result.GetValue(row, 1);

	job.args = Pg::DecodeArray(result.GetValue(row, 2));
	job.env = Pg::DecodeArray(result.GetValue(row, 4));

	if (!result.IsValueNull(row, 3))
		job.syslog_server = result.GetValue(row, 3);

	if (job.id.empty())
		throw std::runtime_error("Job has no id");

	if (job.plan_name.empty())
		throw FormatRuntimeError("Job '%s' has no plan", job.id.c_str());

	return job;
}

static bool
get_and_claim_job(const ChildLogger &logger, const WorkshopJob &job,
		  const char *node_name,
		  Pg::Connection &db,
		  const char *timeout)
{
	logger(6, "attempting to claim job ", job.id);

	if (!pg_claim_job(db, job.id.c_str(), node_name, timeout)) {
		logger(6, "job ", job.id, " was not claimed");
		return false;
	}

	logger(6, "job ", job.id, " claimed");
	return true;
}

/**
 * Copy a string.
 *
 * @return false if the string was not modified.
 */
static bool
copy_string(std::string &dest, std::string &&src) noexcept
{
	if (dest == src)
		return false;

	dest = std::move(src);
	return true;
}

void
WorkshopQueue::SetFilter(std::string &&_plans_include,
			 std::string &&_plans_exclude,
			 std::string &&_plans_lowprio) noexcept
{
	bool r1 = copy_string(plans_include, std::move(_plans_include));
	bool r2 = copy_string(plans_exclude, std::move(_plans_exclude));
	plans_lowprio = std::move(_plans_lowprio);

	if (r1 || r2) {
		if (running)
			interrupt = true;
		else if (db.IsReady())
			Reschedule();
	}
}

void
WorkshopQueue::RunResult(const Pg::Result &result)
{
	for (unsigned row = 0, end = result.GetRowCount();
	     row != end && !IsDisabled() && !interrupt; ++row) {
		auto job = MakeJob(*this, result, row);
		auto plan = handler.GetWorkshopPlan(job.plan_name.c_str());

		if (plan &&
		    handler.CheckWorkshopJob(job, *plan) &&
		    get_and_claim_job(logger, job,
				      GetNodeName(),
				      db, plan->timeout.c_str()))
			handler.StartWorkshopJob(std::move(job),
						 std::move(plan));
	}
}

void
WorkshopQueue::Run2()
{
	int ret;
	bool full = false;

	assert(!IsDisabled());
	assert(running);

	if (plans_include.empty() ||
	    plans_include.compare("{}") == 0 ||
	    plans_exclude.empty())
		return;

	/* check expired jobs from all other nodes except us */

	const auto now = GetEventLoop().SteadyNow();
	if (now >= next_expire_check) {
		next_expire_check = now + std::chrono::seconds(60);

		unsigned n = pg_expire_jobs(db, node_name.c_str());
		if (n > 0) {
			logger(2, "released ", n, " expired jobs");
			pg_notify(db);
		}
	}

	/* query database */

	interrupt = false;

	logger(7, "requesting new jobs from database; "
	       "plans_include=", plans_include,
	       " plans_exclude=", plans_exclude,
	       " plans_lowprio=", plans_lowprio);

	constexpr unsigned MAX_JOBS = 16;
	auto result =
		pg_select_new_jobs(db,
				   plans_include.c_str(), plans_exclude.c_str(),
				   plans_lowprio.c_str(),
				   MAX_JOBS);
	if (!result.IsEmpty()) {
		RunResult(result);

		if (result.GetRowCount() == MAX_JOBS)
			full = true;
	}

	if (!IsDisabled() && !interrupt &&
	    plans_lowprio.compare("{}") != 0) {
		/* now also select plans which are already running */

		logger(7, "requesting new jobs from database II; plans_lowprio=",
		       plans_lowprio);

		result = pg_select_new_jobs(db,
					    plans_lowprio.c_str(),
					    plans_exclude.c_str(),
					    "{}",
					    MAX_JOBS);
		if (!result.IsEmpty()) {
			RunResult(result);

			if (result.GetRowCount() == MAX_JOBS)
				full = true;
		}
	}

	/* update timeout */

	if (IsDisabled()) {
		logger(7, "queue has been disabled");
	} else if (interrupt) {
		/* we have been interrupted: run again in 100ms */
		logger(7, "aborting queue run");

		Reschedule();
	} else if (full) {
		/* 16 is our row limit, and exactly 16 rows were returned - we
		   suspect there may be more.  schedule next queue run in 1
		   second */
		Reschedule();
	} else {
		GetNextScheduled(&ret);
		if (ret >= 0)
			logger(3, "next scheduled job is in ", ret, " seconds");
		else
			ret = 600;

		ScheduleTimer(std::chrono::seconds(ret));
	}
}

void
WorkshopQueue::Run() noexcept
{
	assert(!running);

	if (IsDisabled())
		return;

	ScheduleCheckNotify();

	running = true;

	try {
		Run2();
	} catch (...) {
		db.Error(std::current_exception());
	}

	running = false;
}

void
WorkshopQueue::EnableAdmin() noexcept
{
	assert(!running);

	if (!disabled_admin)
		return;

	disabled_admin = false;

	if (!IsDisabled() && db.IsReady())
		Reschedule();
}

void
WorkshopQueue::EnableFull() noexcept
{
	assert(!running);

	if (!disabled_full)
		return;

	disabled_full = false;

	if (!IsDisabled() && db.IsReady())
		Reschedule();
}

std::chrono::seconds
WorkshopQueue::CheckRateLimit(const char *plan_name,
			      std::chrono::seconds duration,
			      unsigned max_count) noexcept
{
	try {
		return PgCheckRateLimit(db, plan_name, duration, max_count);
	} catch (...) {
		db.Error(std::current_exception());
		return {};
	}
}

bool
WorkshopQueue::SetJobProgress(const WorkshopJob &job, unsigned progress,
			      const char *timeout) noexcept
{
	assert(&job.queue == this);

	logger(5, "job ", job.id, " progress=", progress);

	ScheduleCheckNotify();

	try {
		pg_set_job_progress(db, job.id.c_str(), progress, timeout);
		return true;
	} catch (...) {
		db.Error(std::current_exception());
		return false;
	}
}

void
WorkshopQueue::SetJobEnv(const WorkshopJob &job, const char *more_env)
{
	assert(&job.queue == this);

	ScheduleCheckNotify();

	PgSetEnv(db, job.id.c_str(), more_env);
}

void
WorkshopQueue::RollbackJob(const WorkshopJob &job) noexcept
{
	assert(&job.queue == this);

	logger(6, "rolling back job ", job.id);

	try {
		pg_rollback_job(db, job.id.c_str());
		pg_notify(db);
		ScheduleCheckNotify();
	} catch (...) {
		db.Error(std::current_exception());
	}
}

void
WorkshopQueue::AgainJob(const WorkshopJob &job,
			const char *log,
			std::chrono::seconds delay) noexcept
{
	assert(&job.queue == this);

	logger(6, "rescheduling job ", job.id);

	try {
		if (delay > std::chrono::seconds())
			pg_again_job(db, job.id.c_str(), log, delay);
		else
			pg_rollback_job(db, job.id.c_str());
		pg_notify(db);
		ScheduleCheckNotify();
	} catch (...) {
		db.Error(std::current_exception());
	}
}

void
WorkshopQueue::SetJobDone(const WorkshopJob &job, int status,
			  const char *log) noexcept
{
	assert(&job.queue == this);

	logger(6, "job ", job.id, " done with status ", status);

	try {
		pg_set_job_done(db, job.id.c_str(), status, log);
		ScheduleCheckNotify();
	} catch (...) {
		db.Error(std::current_exception());
	}
}

void
WorkshopQueue::OnConnect()
{
	if (db.GetServerVersion() < 90600)
		throw FormatRuntimeError("PostgreSQL version '%s' is too old, need at least 9.6",
					 db.GetParameterStatus("server_version"));

	static constexpr const char *const required_jobs_columns[] = {
		"enabled",
		"log",
	};

	const char *schema = db.GetSchemaName().empty()
		? "public"
		: db.GetSchemaName().c_str();

	for (const char *name : required_jobs_columns)
		if (!Pg::ColumnExists(db, schema, "jobs", name))
			throw FormatRuntimeError("No column 'jobs.%s'; please migrate the database",
						 name);

	db.Execute("LISTEN new_job");

	if (strcmp(schema, "public") != 0)
		/* for compatibility with future Workshop versions with
		   improved schema support */
		db.Execute(("LISTEN \"" + db.Escape(schema)
			    + ":new_job\"").c_str());

	unsigned ret = pg_release_jobs(db, node_name.c_str());
	if (ret > 0) {
		logger(2, "released ", ret, " stale jobs");
		pg_notify(db);
	}

	Reschedule();
}

void
WorkshopQueue::OnDisconnect() noexcept
{
	logger(4, "disconnected from database");

	timer_event.Cancel();
	check_notify_event.Cancel();
}

void
WorkshopQueue::OnNotify(const char *name)
{
	if (StringEndsWith(name, "new_job"))
		Reschedule();
}

void
WorkshopQueue::OnError(std::exception_ptr e) noexcept
{
	logger(1, e);
}
