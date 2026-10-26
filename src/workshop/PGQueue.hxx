// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

/*
 * SQL to C wrappers.
 */

#pragma once

#include <chrono>
#include <string>

namespace Pg {
class Connection;
class Result;
}

/**
 * Initialize the database connection after it has been established.
 */
void
pg_init(Pg::Connection &db, const char *schema, bool sticky);

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
 * @return the new "time_started" value on success, an empty string
 * if another node has claimed the job earlier
 */
std::string
pg_claim_job(Pg::Connection &db,
	     const char *job_id, const char *node_name,
	     const char *timeout);

/*
 * The following functions which update a claimed job only have an
 * effect if the given node still holds the claim, i.e. if the
 * "node_name" and "time_started" columns match.
 */

/**
 * Throws on error.
 */
void
pg_set_job_progress(Pg::Connection &db, const char *job_id,
		    const char *node_name, const char *time_started,
		    unsigned progress, const char *timeout);

/**
 * Throws on error.
 */
void
PgSetEnv(Pg::Connection &db, const char *job_id,
	 const char *node_name, const char *time_started,
	 const char *more_env);

/**
 * Disassociate from the job, act as if this node had never claimed
 * it.  It will notify the other Workshop nodes.
 *
 * Throws on error.
 */
void
pg_rollback_job(Pg::Connection &db, const char *id,
		const char *node_name, const char *time_started);

/**
 * Like pg_rollback_job(), but also update the "scheduled_time"
 * column.
 *
 * Throws on error.
 */
void
pg_again_job(Pg::Connection &db, const char *id,
	     const char *node_name, const char *time_started,
	     const char *log, std::chrono::seconds delay);

/**
 * Throws on error.
 */
void
pg_set_job_done(Pg::Connection &db, const char *id,
		const char *node_name, const char *time_started,
		int status, const char *log);

void
PgAddJobCpuUsage(Pg::Connection &db, const char *id,
		 const char *node_name, const char *time_started,
		 std::chrono::microseconds cpu_usage);

unsigned
PgReapFinishedJobs(Pg::Connection &db, const char *plan_name,
		   const char *reap_finished);
