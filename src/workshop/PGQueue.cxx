// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "PGQueue.hxx"
#include "pg/Connection.hxx"
#include "pg/Reflection.hxx"
#include "lib/fmt/ToBuffer.hxx"

#include <fmt/core.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

using std::string_view_literals::operator""sv;

void
pg_init(Pg::Connection &db, const char *schema, bool sticky)
{
	/* if the "stdin" column does not exist, assume it's all
	   NULL */
	const std::string_view stdin_column = Pg::ColumnExists(db, schema, "jobs", "stdin")
		? "stdin"sv
		: "NULL"sv;

	const std::string_view set_time_modified = Pg::ColumnExists(db, schema, "jobs", "time_modified")
		? ", time_modified=now()"sv
		: ""sv;

	const std::string_view sticky_id_column = sticky
		? "sticky_id"sv
		: "NULL"sv;

	const std::string_view sticky_id_check = sticky
		? "(sticky_id IS NULL OR NOT EXISTS (SELECT 1 FROM sticky_non_local WHERE sticky_non_local.sticky_id=jobs.sticky_id))"sv
		: "TRUE"sv;

	/* ignore jobs which are scheduled deep into the future; some
	   Workshop clients (such as URO) do this, and it slows down
	   the PostgreSQL query */
	db.Prepare("next_scheduled_job", fmt::format(R"SQL(
SELECT EXTRACT(EPOCH FROM (MIN(scheduled_time) - now()))
FROM jobs
WHERE node_name IS NULL AND time_done IS NULL AND exit_status IS NULL
  AND scheduled_time IS NOT NULL
  AND scheduled_time < now() + '1 year'::interval
  AND plan_name = ANY ($1::TEXT[])
  AND enabled
  AND {}
)SQL", sticky_id_check).c_str(), 1);

	db.Prepare("select_new_jobs", fmt::format(R"SQL(
SELECT id,plan_name,{},args,env,{}
  FROM jobs
WHERE node_name IS NULL
  AND time_done IS NULL AND exit_status IS NULL
  AND (scheduled_time IS NULL OR now() >= scheduled_time)
  AND plan_name = ANY ($1::TEXT[])
  AND plan_name <> ALL ($2::TEXT[] || $3::TEXT[])
  AND enabled
  AND {}
ORDER BY priority, time_created
LIMIT $4
)SQL", sticky_id_column, stdin_column, sticky_id_check).c_str(),
		   4);

	db.Prepare("check_rate_limit", R"SQL(
SELECT EXTRACT(EPOCH FROM time_started + $2::interval - now()) FROM jobs
WHERE plan_name=$1 AND time_started >= now() - $2::interval
ORDER BY time_started DESC
LIMIT 1 OFFSET $3
)SQL",
		   3);

	db.Prepare("claim_job", fmt::format(R"SQL(
UPDATE jobs
SET node_name=$1, node_timeout=now()+$3::INTERVAL, time_started=now()
 {}
WHERE id=$2 AND node_name IS NULL AND enabled
)SQL", set_time_modified).c_str(),
		   3);

	db.Prepare("set_job_progress", fmt::format(R"SQL(
UPDATE jobs
SET progress=$2, node_timeout=now()+$3::INTERVAL
 {}
WHERE id=$1
)SQL", set_time_modified).c_str(),
		   3);

	db.Prepare("set_job_done", fmt::format(R"SQL(
UPDATE jobs
SET time_done=now(), progress=100, exit_status=$2, log=$3
 {}
WHERE id=$1
)SQL", set_time_modified).c_str(),
		   3);

	db.Prepare("add_cpu_usage", R"SQL(
UPDATE jobs
SET cpu_usage=COALESCE(cpu_usage, '0'::interval)+$2::interval
WHERE id=$1
)SQL",
		   2);

	db.Prepare("release_jobs", fmt::format(R"SQL(
UPDATE jobs
SET node_name=NULL, node_timeout=NULL, progress=0
 {}
WHERE node_name=$1 AND time_done IS NULL AND exit_status IS NULL
)SQL", set_time_modified).c_str(),
		   1);

	db.Prepare("expire_jobs", fmt::format(R"SQL(
UPDATE jobs
SET node_name=NULL, node_timeout=NULL, progress=0
 {}
WHERE time_done IS NULL AND exit_status IS NULL AND
node_name IS NOT NULL AND node_name <> $1 AND
node_timeout IS NOT NULL AND now() > node_timeout
)SQL", set_time_modified).c_str(),
		   1);

	db.Prepare("set_env", R"SQL(
UPDATE jobs
SET env=ARRAY(SELECT x FROM (SELECT unnest(env) as x) AS y WHERE x NOT LIKE $3)||ARRAY[$2]::varchar[]
WHERE id=$1
)SQL",
		   3);

	db.Prepare("rollback_job", fmt::format(R"SQL(
UPDATE jobs
SET node_name=NULL, node_timeout=NULL, progress=0
 {}
WHERE id=$1 AND node_name IS NOT NULL
AND time_done IS NULL
)SQL", set_time_modified).c_str(),
		   1);

	db.Prepare("again_job", fmt::format(R"SQL(
UPDATE jobs
SET node_name=NULL, node_timeout=NULL, progress=0
, log=$3
, scheduled_time=NOW() + $2 * '1 second'::interval
 {}
WHERE id=$1 AND node_name IS NOT NULL
AND time_done IS NULL
)SQL", set_time_modified).c_str(),
		   3);

	db.Prepare("reap_finished_jobs", R"SQL(
DELETE FROM jobs
WHERE plan_name=$1
 AND time_done IS NOT NULL AND time_done < now() - $2::interval
)SQL",
		   2);
}

void
pg_notify(Pg::Connection &db)
{
	db.Execute("NOTIFY new_job");
}

unsigned
pg_release_jobs(Pg::Connection &db, const char *node_name)
{
	const auto result = db.ExecutePrepared("release_jobs", node_name);
	return result.GetAffectedRows();
}

unsigned
pg_expire_jobs(Pg::Connection &db, const char *except_node_name)
{
	const auto result = db.ExecutePrepared("expire_jobs", except_node_name);
	return result.GetAffectedRows();
}

bool
pg_next_scheduled_job(Pg::Connection &db,
		      const char *plans_include,
		      long *span_r)
{
	assert(plans_include != nullptr && *plans_include == '{');

	const auto result = db.ExecutePrepared("next_scheduled_job", plans_include);
	if (result.IsEmpty())
		return false;

	const char *value = result.GetValue(0, 0);
	if (value == nullptr || *value == 0)
		return false;

	*span_r = strtol(value, nullptr, 0);
	return true;
}

Pg::Result
pg_select_new_jobs(Pg::Connection &db,
		   const char *plans_include, const char *plans_exclude,
		   const char *plans_lowprio,
		   unsigned limit)
{
	assert(plans_include != nullptr && *plans_include == '{');
	assert(plans_exclude != nullptr && *plans_exclude == '{');
	assert(plans_lowprio != nullptr && *plans_lowprio == '{');

	return db.ExecutePrepared("select_new_jobs",
				  plans_include, plans_exclude, plans_lowprio,
				  limit);
}

std::chrono::seconds
PgCheckRateLimit(Pg::Connection &db, const char *plan_name,
		 std::chrono::seconds duration, unsigned max_count)
{
	const auto value = db.ExecutePrepared("check_rate_limit", plan_name, duration.count(),
					      max_count - 1)
		.GetOnlyStringChecked();
	if (value.empty())
		return {};

	return std::chrono::seconds(strtoul(value.c_str(), nullptr, 10));
}

bool
pg_claim_job(Pg::Connection &db,
	     const char *job_id, const char *node_name,
	     const char *timeout)
{
	const auto result = db.ExecutePrepared("claim_job", node_name, job_id, timeout);
	return result.GetAffectedRows() > 0;
}

void
pg_set_job_progress(Pg::Connection &db, const char *job_id,
		    unsigned progress, const char *timeout)
{
	const auto result =
		db.ExecutePrepared("set_job_progress",
				   job_id, progress, timeout);
	if (result.GetAffectedRows() < 1)
		throw std::runtime_error("No matching job");
}

void
PgSetEnv(Pg::Connection &db, const char *job_id, const char *more_env)
{
	const char *eq = strchr(more_env, '=');
	if (eq == nullptr || eq == more_env)
		throw std::runtime_error("Malformed environment variable");

	const std::string_view name{more_env, eq};

	/* for filtering out old environment variables with the same
	   name */
	const auto like = fmt::format("{}=%"sv, name);

	const auto result = db.ExecutePrepared("set_env", job_id, more_env, like.c_str());
	if (result.GetAffectedRows() < 1)
		throw std::runtime_error("No matching job");
}

void
pg_rollback_job(Pg::Connection &db, const char *id)
{
	const auto result = db.ExecutePrepared("rollback_job", id);
	if (result.GetAffectedRows() < 1)
		throw std::runtime_error("No matching job");
}

void
pg_again_job(Pg::Connection &db, const char *id, const char *log,
	     std::chrono::seconds delay)
{
	const auto result = db.ExecutePrepared("again_job", id, delay.count(), log);
	if (result.GetAffectedRows() < 1)
		throw std::runtime_error("No matching job");
}

void
pg_set_job_done(Pg::Connection &db, const char *id, int status,
		const char *log)
{
	const auto result = db.ExecutePrepared("set_job_done", id, status, log);
	if (result.GetAffectedRows() < 1)
		throw std::runtime_error("No matching job");
}

void
PgAddJobCpuUsage(Pg::Connection &db, const char *id,
		 std::chrono::microseconds cpu_usage)
{
	const auto cpu_usage_s = FmtBuffer<64>("{} microseconds", cpu_usage.count());

	const auto result = db.ExecutePrepared("add_cpu_usage",
					       id, cpu_usage_s.c_str());
	if (result.GetAffectedRows() < 1)
		throw std::runtime_error("No matching job");
}

unsigned
PgReapFinishedJobs(Pg::Connection &db, const char *plan_name,
		   const char *reap_finished)
{
	return db.ExecutePrepared("reap_finished_jobs", plan_name, reap_finished)
		.GetAffectedRows();
}
