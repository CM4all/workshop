// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * SQL to C wrappers.
 */

#ifndef __QUEUE_PG_H
#define __QUEUE_PG_H

#include <chrono>

namespace Pg {
class Connection;
class Result;
}

/**
 * Throws on error.
 */
void
pg_notify(Pg::Connection &db);

/**
 * Throws on error.
 */
unsigned
pg_release_jobs(Pg::Connection &db, const char *node_name);

/**
 * Throws on error.
 */
unsigned
pg_expire_jobs(Pg::Connection &db, const char *except_node_name);

/**
 * Throws on error.
 */
bool
pg_next_scheduled_job(Pg::Connection &db,
		      const char *plans_include,
		      long *span_r);

/**
 * Throws on error.
 */
Pg::Result
pg_select_new_jobs(Pg::Connection &db,
		   const char *plans_include, const char *plans_exclude,
		   const char *plans_lowprio,
		   unsigned limit);

/**
 * Checks if the given rate limit was reached/exceeded.
 *
 * Throws on error.
 *
 * @return a positive duration we have to wait until the rate falls
 * below the limit and a new job can be started, or a non-positive
 * value if the rate limits is not yet reached
 */
std::chrono::seconds
PgCheckRateLimit(Pg::Connection &db, const char *plan_name,
		 std::chrono::seconds duration, unsigned max_count);

/**
 * Throws on error.
 *
 * @return true on success, false if another node has claimed the job
 * earlier
 */
bool
pg_claim_job(Pg::Connection &db,
	     const char *job_id, const char *node_name,
	     const char *timeout);

/**
 * Throws on error.
 */
void
pg_set_job_progress(Pg::Connection &db, const char *job_id, unsigned progress,
		    const char *timeout);

/**
 * Throws on error.
 */
void
PgSetEnv(Pg::Connection &db, const char *job_id, const char *more_env);

/**
 * Throws on error.
 */
void
pg_rollback_job(Pg::Connection &db, const char *id);

/**
 * Like pg_rollback_job(), but also update the "scheduled_time"
 * column.
 *
 * Throws on error.
 */
void
pg_again_job(Pg::Connection &db, const char *id, const char *log,
	     std::chrono::seconds delay);

/**
 * Throws on error.
 */
void
pg_set_job_done(Pg::Connection &db, const char *id, int status,
		const char *log);

void
PgAddJobCpuUsage(Pg::Connection &db, const char *id,
		 std::chrono::microseconds cpu_usage);

unsigned
PgReapFinishedJobs(Pg::Connection &db, const char *plan_name,
		   const char *reap_finished);

#endif
