// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Queue.hxx"
#include "Job.hxx"
#include "Result.hxx"
#include "CalculateNextRun.hxx"
#include "StickyTable.hxx"
#include "pg/Reflection.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/StringAPI.hxx"

#include <chrono>

#include <string.h>
#include <stdlib.h>

using std::string_view_literals::operator""sv;

CronQueue::CronQueue(const Logger &parent_logger,
		     EventLoop &event_loop, const char *_node_name,
		     Pg::Config &&_db_config,
		     bool _sticky,
		     Callback _callback) noexcept
	:node_name(_node_name),
	 logger(parent_logger, "queue"),
	 db(event_loop, std::move(_db_config), *this),
	 callback(_callback),
	 check_notify_event(event_loop, BIND_THIS_METHOD(CheckNotify)),
	 scheduler_timer(event_loop, BIND_THIS_METHOD(RunScheduler)),
	 claim_timer(event_loop, BIND_THIS_METHOD(RunClaim)),
	 sticky(_sticky)
{
}

CronQueue::~CronQueue() noexcept = default;

inline void
CronQueue::Prepare()
{
	const char *const schema = db.GetEffectiveSchemaName();
	const bool have_sticky_id = sticky && Pg::ColumnExists(db, schema, "cronjobs", "sticky_id");

	if (have_sticky_id)
		StickyTable::Init(db);

	db.Prepare("release_stale", R"SQL(
UPDATE cronjobs
SET node_name=NULL, node_timeout=NULL, next_run=NULL
WHERE node_name=$1
)SQL",
		   1);

	db.Prepare("claim_job", R"SQL(
UPDATE cronjobs
SET node_name=$2, node_timeout=now()+$3::INTERVAL
WHERE id=$1 AND enabled AND node_name IS NULL
)SQL",
		   3);

	db.Prepare("finish_job", R"SQL(
UPDATE cronjobs
SET node_name=NULL, node_timeout=NULL, last_run=now(), next_run=NULL
WHERE id=$1 AND node_name=$2
)SQL",
		   2);

	db.Prepare("insert_result", R"SQL(
INSERT INTO cronresults(cronjob_id, node_name, start_time, exit_status, log)
VALUES($1, $2, $3, $4, $5)
)SQL",
		   5);

	const std::string_view sticky_id_check = have_sticky_id
		? "(sticky_id IS NULL OR NOT EXISTS (SELECT 1 FROM sticky_non_local WHERE sticky_non_local.sticky_id=cronjobs.sticky_id))"sv
		: "TRUE"sv;

	db.Prepare("find_earliest_pending", fmt::format(R"SQL(
SELECT EXTRACT(EPOCH FROM (MIN(next_run) - now())) FROM cronjobs
WHERE enabled AND next_run IS NOT NULL AND next_run != 'infinity' AND node_name IS NULL
 AND {}
)SQL", sticky_id_check).c_str(),
		   0);

	const std::string_view sticky_id_column = have_sticky_id
		? "sticky_id"sv
		: "NULL"sv;

	db.Prepare("check_pending", fmt::format(R"SQL(
SELECT id, account_id, command, translate_param, notification, {}
FROM cronjobs
WHERE enabled AND next_run<=now()
 AND node_name IS NULL
 AND {}
ORDER BY next_run
LIMIT 1
)SQL", sticky_id_column, sticky_id_check).c_str(),
		   0);

	InitCalculateNextRun(db);
}

void
CronQueue::CheckEnabled() noexcept
{
	if (IsEnabled()) {
		if (!db.IsDefined())
			db.Connect();
		else if (db.IsReady())
			ScheduleClaim();
	} else {
		if (db.IsDefined() && !db.IsReady())
			db.Disconnect();

		// TODO disconnect if idle?
	}
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
	const auto result = db.ExecutePrepared("release_stale", node_name.c_str());

	unsigned n = result.GetAffectedRows();
	if (n > 0)
		logger(3, "Released ", n, " stale cronjobs");
}

void
CronQueue::InsertStickyNonLocal(const char *sticky_id) noexcept
try {
	if (!db.IsReady())
		return;

	StickyTable::InsertNonLocal(db, sticky_id);
} catch (...) {
	db.CheckError(std::current_exception());
}

void
CronQueue::FlushSticky() noexcept
try {
	if (!db.IsReady())
		return;

	StickyTable::Flush(db);
} catch (...) {
	db.CheckError(std::current_exception());
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
	const auto result = db.ExecutePrepared("find_earliest_pending");

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

	while (true) {
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

			if (!CheckPending())
				break;
		} catch (...) {
			db.CheckError(std::current_exception());
			return;
		}
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

	const auto r = db.ExecutePrepared("claim_job", job.id.c_str(),
					  node_name.c_str(), timeout);
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

	const auto r = db.ExecutePrepared("finish_job", job.id.c_str(), node_name.c_str());
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

	db.ExecutePrepared("insert_result",
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

	enum Columns {
		ID,
		ACCOUNT_ID,
		COMMAND,
		TRANSLATE_PARAM,
		NOTIFICATION,
		STICKY_ID,
	};

	const auto result = db.ExecutePrepared("check_pending");
	if (result.IsEmpty())
		return false;

	for (const auto &row : result) {
		CronJob job{
			.id = std::string{row.GetValueView(ID)},
			.account_id = std::string{row.GetValueView(ACCOUNT_ID)},
			.command = std::string{row.GetValueView(COMMAND)},
			.translate_param = std::string{row.GetValueView(TRANSLATE_PARAM)},
			.notification = std::string{row.GetValueView(NOTIFICATION)},
			.sticky_id = std::string{row.GetValueView(STICKY_ID)},
		};

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
		throw FmtRuntimeError("PostgreSQL version {:?} is too old, need at least 9.6",
				      db.GetParameterStatus("server_version"));

	Prepare();

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
