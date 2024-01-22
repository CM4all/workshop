// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Queue.hxx"
#include "Job.hxx"
#include "Result.hxx"
#include "CalculateNextRun.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/StringAPI.hxx"

#include <chrono>

#include <string.h>
#include <stdlib.h>

CronQueue::CronQueue(const Logger &parent_logger,
		     EventLoop &event_loop, const char *_node_name,
		     const char *conninfo, const char *schema,
		     Callback _callback) noexcept
	:node_name(_node_name),
	 logger(parent_logger, "queue"),
	 db(event_loop, conninfo, schema, *this),
	 callback(_callback),
	 check_notify_event(event_loop, BIND_THIS_METHOD(CheckNotify)),
	 scheduler_timer(event_loop, BIND_THIS_METHOD(RunScheduler)),
	 claim_timer(event_loop, BIND_THIS_METHOD(RunClaim))
{
}

CronQueue::~CronQueue() noexcept = default;

void
CronQueue::CheckEnabled() noexcept
{
	if (IsEnabled() && db.IsReady())
		ScheduleClaim();
}

void
CronQueue::SetStateEnabled(bool _enabled) noexcept
{
	if (_enabled == enabled_state)
		return;

	logger.Fmt(2, "SetStateEnabled {}", _enabled);

	enabled_state = _enabled;
	CheckEnabled();
}

void
CronQueue::EnableAdmin() noexcept
{
	if (enabled_admin)
		return;

	enabled_admin = true;
	CheckEnabled();
}

void
CronQueue::EnableFull() noexcept
{
	if (!disabled_full)
		return;

	disabled_full = false;
	CheckEnabled();
}

void
CronQueue::ReleaseStale()
{
	const auto result = db.ExecuteParams("UPDATE cronjobs "
					     "SET node_name=NULL, node_timeout=NULL, next_run=NULL "
					     "WHERE node_name=$1",
					     node_name.c_str());

	unsigned n = result.GetAffectedRows();
	if (n > 0)
		logger(3, "Released ", n, " stale cronjobs");
}

void
CronQueue::RunScheduler() noexcept
{
	logger(4, "scheduler");

	try {
		if (!CalculateNextRun(logger, db))
			ScheduleScheduler(false);

		ScheduleCheckNotify();
	} catch (...) {
		db.CheckError(std::current_exception());
	}
}

void
CronQueue::ScheduleScheduler(bool immediately) noexcept
{
	Event::Duration d;
	if (immediately) {
		d = d.zero();
	} else {
		/* randomize the scheduler to reduce race conditions with
		   other nodes */
		d = std::chrono::microseconds(random() % 5000000);
	}

	scheduler_timer.Schedule(d);
}

static std::chrono::seconds
FindEarliestPending(Pg::Connection &db)
{
	const auto result =
		db.Execute("SELECT EXTRACT(EPOCH FROM (MIN(next_run) - now())) FROM cronjobs "
			   "WHERE enabled AND next_run IS NOT NULL AND next_run != 'infinity' AND node_name IS NULL");

	if (result.IsEmpty() || result.IsValueNull(0, 0))
		/* no matching cronjob; disable the timer, and wait for the
		   next PostgreSQL notify */
		return std::chrono::seconds::max();

	const char *s = result.GetValue(0, 0);
	char *endptr;
	long long value = strtoull(s, &endptr, 10);
	if (endptr == s)
		return std::chrono::minutes(1);

	return std::min<std::chrono::seconds>(std::chrono::seconds(value),
					      std::chrono::hours(24));
}

void
CronQueue::RunClaim() noexcept
{
	if (!IsEnabled())
		return;

	logger(4, "claim");

	try {
		auto delta = FindEarliestPending(db);
		if (delta == delta.max())
			return;

		if (delta > delta.zero()) {
			/* randomize the claim to reduce race conditions with
			   other nodes */
			Event::Duration r = std::chrono::microseconds(random() % 30000000);
			claim_timer.Schedule(Event::Duration(delta) + r);
			return;
		}

		CheckPending();
	} catch (...) {
		db.CheckError(std::current_exception());
		return;
	}

	ScheduleClaim();
	ScheduleCheckNotify();
}

void
CronQueue::ScheduleClaim() noexcept
{
	claim_timer.Schedule(std::chrono::seconds(1));
}

bool
CronQueue::Claim(const CronJob &job) noexcept
try {
	const char *timeout = "5 minutes";

	const auto r =
		db.ExecuteParams("UPDATE cronjobs "
				 "SET node_name=$2, node_timeout=now()+$3::INTERVAL "
				 "WHERE id=$1 AND enabled AND node_name IS NULL",
				 job.id.c_str(),
				 node_name.c_str(),
				 timeout);
	if (r.GetAffectedRows() == 0) {
		logger(3, "Lost race to run job '", job.id, "'");
		return false;
	}

	return true;
} catch (...) {
	db.CheckError(std::current_exception());
	return false;
}

void
CronQueue::Finish(const CronJob &job) noexcept
try {
	ScheduleCheckNotify();

	const auto r =
		db.ExecuteParams("UPDATE cronjobs "
				 "SET node_name=NULL, node_timeout=NULL, last_run=now(), next_run=NULL "
				 "WHERE id=$1 AND node_name=$2",
				 job.id.c_str(),
				 node_name.c_str());
	if (r.GetAffectedRows() == 0) {
		logger(3, "Lost race to finish job '", job.id, "'");
		return;
	}
} catch (...) {
	db.CheckError(std::current_exception());
}

void
CronQueue::InsertResult(const CronJob &job, const char *start_time,
			const CronResult &result) noexcept
try {
	ScheduleCheckNotify();

	db.ExecuteParams("INSERT INTO cronresults(cronjob_id, node_name, start_time, exit_status, log) "
			 "VALUES($1, $2, $3, $4, $5)",
			 job.id.c_str(),
			 node_name.c_str(),
			 start_time,
			 result.exit_status,
			 result.log.c_str());
} catch (...) {
	db.CheckError(std::current_exception());
}

bool
CronQueue::CheckPending()
{
	if (!IsEnabled())
		return false;

	const auto result =
		db.Execute("SELECT id, account_id, command, translate_param, notification "
			   "FROM cronjobs WHERE enabled AND next_run<=now() "
			   "AND node_name IS NULL "
			   "LIMIT 1");
	if (result.IsEmpty())
		return false;

	for (const auto &row : result) {
		CronJob job;
		job.id = row.GetValue(0);
		job.account_id = row.GetValue(1);
		job.command = row.GetValue(2);
		job.translate_param = row.GetValue(3);
		job.notification = row.GetValue(4);

		callback(std::move(job));

		if (!IsEnabled())
			return false;
	}

	return true;
}

void
CronQueue::OnConnect()
{
	if (db.GetServerVersion() < 90600)
		throw FmtRuntimeError("PostgreSQL version '{}' is too old, need at least 9.6",
				      db.GetParameterStatus("server_version"));

	db.Execute("LISTEN cronjobs_modified");
	db.Execute("LISTEN cronjobs_scheduled");

	/* internally, all time stamps should be UTC, and PostgreSQL
	   should not mangle those time stamps to the time zone that our
	   process happens to be configured for */
	try {
		db.Execute("SET timezone='UTC'");
	} catch (...) {
		logger(1, "SET timezone failed: ", std::current_exception());
	}

	ReleaseStale();

	ScheduleScheduler(true);
	ScheduleClaim();
	ScheduleCheckNotify();
}

void
CronQueue::OnDisconnect() noexcept
{
	logger(4, "disconnected from database");

	check_notify_event.Cancel();
	scheduler_timer.Cancel();
	claim_timer.Cancel();
}

void
CronQueue::OnNotify(const char *name)
{
	if (StringIsEqual(name, "cronjobs_modified"))
		ScheduleScheduler(false);
	else if (StringIsEqual(name, "cronjobs_scheduled"))
		ScheduleClaim();
}

void
CronQueue::OnError(std::exception_ptr e) noexcept
{
	logger(1, e);
}
