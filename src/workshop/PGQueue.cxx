// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PGQueue.hxx"
#include "pg/Connection.hxx"
#include "lib/fmt/ToBuffer.hxx"

#include <fmt/core.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

using std::string_view_literals::operator""sv;

void
pg_init(Pg::Connection &db)
{
	db.Prepare("select_new_jobs", R"SQL(
SELECT id,plan_name,args,env,stdin
  FROM jobs
WHERE node_name IS NULL
  AND time_done IS NULL AND exit_status IS NULL
  AND (scheduled_time IS NULL OR now() >= scheduled_time)
  AND plan_name = ANY ($1::TEXT[])
  AND plan_name <> ALL ($2::TEXT[] || $3::TEXT[])
  AND enabled
ORDER BY priority, time_created
LIMIT $4
)SQL",
		   4);

	db.Prepare("claim_job", R"SQL(
UPDATE jobs
SET node_name=$1, node_timeout=now()+$3::INTERVAL, time_started=now()
WHERE id=$2 AND node_name IS NULL AND enabled
)SQL",
		   3);

	db.Prepare("set_job_progress", R"SQL(
UPDATE jobs
SET progress=$2, node_timeout=now()+$3::INTERVAL
WHERE id=$1
)SQL",
		   3);

	db.Prepare("set_job_done", R"SQL(
UPDATE jobs
SET time_done=now(), progress=100, exit_status=$2, log=$3
WHERE id=$1
)SQL",
		   3);
}

void
pg_notify(Pg::Connection &db)
{
	db.Execute("NOTIFY new_job");
}

unsigned
pg_release_jobs(Pg::Connection &db, const char *node_name)
{
	const auto result =
		db.ExecuteParams("UPDATE jobs "
				 "SET node_name=NULL, node_timeout=NULL, progress=0 "
				 "WHERE node_name=$1 AND time_done IS NULL AND exit_status IS NULL",
				 node_name);
	return result.GetAffectedRows();
}

unsigned
pg_expire_jobs(Pg::Connection &db, const char *except_node_name)
{
	const auto result =
		db.ExecuteParams("UPDATE jobs "
				 "SET node_name=NULL, node_timeout=NULL, progress=0 "
				 "WHERE time_done IS NULL AND exit_status IS NULL AND "
				 "node_name IS NOT NULL AND node_name <> $1 AND "
				 "node_timeout IS NOT NULL AND now() > node_timeout",
				 except_node_name);
	return result.GetAffectedRows();
}

bool
pg_next_scheduled_job(Pg::Connection &db,
		      const char *plans_include,
		      long *span_r)
{
	assert(plans_include != nullptr && *plans_include == '{');

	const char *sql = "SELECT EXTRACT(EPOCH FROM (MIN(scheduled_time) - now())) "
		"FROM jobs WHERE node_name IS NULL AND time_done IS NULL AND exit_status IS NULL "
		"AND scheduled_time IS NOT NULL "

		/* ignore jobs which are scheduled deep into
		   the future; some Workshop clients (such as
		   URO) do this, and it slows down the
		   PostgreSQL query */
		"AND scheduled_time < now() + '1 year'::interval "

		"AND plan_name = ANY ($1::TEXT[]) "
		"AND enabled";

	const auto result = db.ExecuteParams(sql, plans_include);
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
	const char *sql = "SELECT EXTRACT(EPOCH FROM time_started + $2::interval - now()) FROM jobs "
		" WHERE plan_name=$1 AND time_started >= now() - $2::interval"
		" ORDER BY time_started DESC"
		" LIMIT 1 OFFSET $3";

	const auto value = db.ExecuteParams(sql, plan_name, duration.count(),
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

	const auto result =
		db.ExecuteParams("UPDATE jobs "
				 "SET env=ARRAY(SELECT x FROM (SELECT unnest(env) as x) AS y WHERE x NOT LIKE $3)||ARRAY[$2]::varchar[] "
				 "WHERE id=$1",
				 job_id, more_env, like.c_str());
	if (result.GetAffectedRows() < 1)
		throw std::runtime_error("No matching job");
}

void
pg_rollback_job(Pg::Connection &db, const char *id)
{
	const auto result =
		db.ExecuteParams("UPDATE jobs "
				 "SET node_name=NULL, node_timeout=NULL, progress=0 "
				 "WHERE id=$1 AND node_name IS NOT NULL "
				 "AND time_done IS NULL",
				 id);
	if (result.GetAffectedRows() < 1)
		throw std::runtime_error("No matching job");
}

void
pg_again_job(Pg::Connection &db, const char *id, const char *log,
	     std::chrono::seconds delay)
{
	const auto result =
		db.ExecuteParams("UPDATE jobs "
				 "SET node_name=NULL, node_timeout=NULL, progress=0 "
				 ", log=$3"
				 ", scheduled_time=NOW() + $2 * '1 second'::interval "
				 "WHERE id=$1 AND node_name IS NOT NULL "
				 "AND time_done IS NULL",
				 id, delay.count(), log);
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

	const auto result =
		db.ExecuteParams("UPDATE jobs "
				 "SET cpu_usage=COALESCE(cpu_usage, '0'::interval)+$2::interval "
				 "WHERE id=$1",
				 id, cpu_usage_s.c_str());
	if (result.GetAffectedRows() < 1)
		throw std::runtime_error("No matching job");
}

unsigned
PgReapFinishedJobs(Pg::Connection &db, const char *plan_name,
		   const char *reap_finished)
{
	const char *const sql = R"SQL(
DELETE FROM jobs
WHERE plan_name=$1
 AND time_done IS NOT NULL AND time_done < now() - $2::interval
)SQL";

	return db.ExecuteParams(sql, plan_name, reap_finished)
		.GetAffectedRows();
}
