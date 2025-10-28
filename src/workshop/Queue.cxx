// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Queue.hxx"
#include "PGQueue.hxx"
#include "Job.hxx"
#include "Plan.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "pg/Array.hxx"
#include "pg/Hex.hxx"
#include "pg/Reflection.hxx"
#include "event/Loop.hxx"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"

#include <fmt/core.h>

#include <stdexcept>

#include <sys/types.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
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
	job.env = Pg::DecodeArray(result.GetValue(row, 3));

	if (!result.IsValueNull(row, 4))
		job.stdin = Pg::DecodeHex(result.GetValueView(row, 4));

	if (job.id.empty())
		throw std::runtime_error("Job has no id");

	if (job.plan_name.empty())
		throw FmtRuntimeError("Job '{}' has no plan", job.id);

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
	     row != end && IsEnabled() && !interrupt; ++row) {
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

	assert(IsEnabled());
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

	if (IsEnabled() && !interrupt &&
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

	if (!IsEnabled()) {
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

	if (!IsEnabled())
		return;

	ScheduleCheckNotify();

	running = true;

	try {
		Run2();
	} catch (...) {
		db.CheckError(std::current_exception());
	}

	running = false;
}

void
WorkshopQueue::CheckEnabled() noexcept
{
	if (IsEnabled() && db.IsReady())
		Reschedule();
}

void
WorkshopQueue::SetStateEnabled(bool _enabled) noexcept
{
	if (_enabled == enabled_state)
		return;

	logger.Fmt(2, "SetStateEnabled {}", _enabled);

	enabled_state = _enabled;
	CheckEnabled();
}

void
WorkshopQueue::EnableAdmin() noexcept
{
	assert(!running);

	if (enabled_admin)
		return;

	enabled_admin = true;

	CheckEnabled();
}

void
WorkshopQueue::EnableFull() noexcept
{
	assert(!running);

	if (!disabled_full)
		return;

	disabled_full = false;

	CheckEnabled();
}

std::chrono::seconds
WorkshopQueue::CheckRateLimit(const char *plan_name,
			      std::chrono::seconds duration,
			      unsigned max_count) noexcept
{
	try {
		return PgCheckRateLimit(db, plan_name, duration, max_count);
	} catch (...) {
		db.CheckError(std::current_exception());
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
		db.CheckError(std::current_exception());
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
		db.CheckError(std::current_exception());
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
		db.CheckError(std::current_exception());
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
		db.CheckError(std::current_exception());
	}
}

void
WorkshopQueue::AddJobCpuUsage(const WorkshopJob &job,
			      std::chrono::microseconds cpu_usage) noexcept
{
	assert(&job.queue == this);

	try {
		PgAddJobCpuUsage(db, job.id.c_str(), cpu_usage);
	} catch (...) {
		db.CheckError(std::current_exception());
	}
}

unsigned
WorkshopQueue::ReapFinishedJobs(const char *plan_name,
				const char *reap_finished) noexcept
{
	if (!db.IsReady())
		/* can't reap finished jobs because we currently have
		   no database connection */
		return 0;

	try {
		return PgReapFinishedJobs(db, plan_name, reap_finished);
	} catch (...) {
		db.CheckError(std::current_exception());
		return 0;
	}
}

void
WorkshopQueue::OnConnect()
{
	if (db.GetServerVersion() < 90600)
		throw FmtRuntimeError("PostgreSQL version '{}' is too old, need at least 9.6",
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
			throw FmtRuntimeError("No column 'jobs.{}'; please migrate the database",
					      name);

	pg_init(db, schema);

	db.Execute("LISTEN new_job");

	if (!StringIsEqual(schema, "public"))
		/* for compatibility with future Workshop versions with
		   improved schema support */
		db.Execute(fmt::format("LISTEN \"{}:new_job\"", schema).c_str());

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
