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

#include "PGQueue.hxx"
#include "pg/Connection.hxx"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

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

	const char *sql = "SELECT id,plan_name,args,syslog_server,env "
		"FROM jobs "
		"WHERE node_name IS NULL "
		"AND time_done IS NULL AND exit_status IS NULL "
		"AND (scheduled_time IS NULL OR now() >= scheduled_time) "
		"AND plan_name = ANY ($1::TEXT[]) "
		"AND plan_name <> ALL ($2::TEXT[] || $3::TEXT[]) "
		"AND enabled "
		"ORDER BY priority, time_created "
		"LIMIT $4";

	return db.ExecuteParams(sql,
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
	const char *sql = "UPDATE jobs "
		"SET node_name=$1, node_timeout=now()+$3::INTERVAL, time_started=now() "
		"WHERE id=$2 AND node_name IS NULL"
		" AND enabled";

	const auto result = db.ExecuteParams(sql, node_name, job_id, timeout);
	return result.GetAffectedRows() > 0;
}

void
pg_set_job_progress(Pg::Connection &db, const char *job_id,
		    unsigned progress, const char *timeout)
{
	const auto result =
		db.ExecuteParams("UPDATE jobs "
				 "SET progress=$2, node_timeout=now()+$3::INTERVAL "
				 "WHERE id=$1",
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

	/* for filtering out old environment variables with the same
	   name */
	std::string like(more_env, eq + 1);
	like.push_back('%');

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
	const auto result =
		db.ExecuteParams("UPDATE jobs "
				 "SET time_done=now(), progress=100, exit_status=$2, log=$3 "
				 "WHERE id=$1",
				 id, status, log);
	if (result.GetAffectedRows() < 1)
		throw std::runtime_error("No matching job");
}

void
PgAddJobCpuUsage(Pg::Connection &db, const char *id,
		 std::chrono::microseconds cpu_usage)
{
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%" PRIu64 " microseconds",
		 cpu_usage.count());

	const auto result =
		db.ExecuteParams("UPDATE jobs "
				 "SET cpu_usage=COALESCE(cpu_usage, '0'::interval)+$2::interval "
				 "WHERE id=$1",
				 id, (const char *)buffer);
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
