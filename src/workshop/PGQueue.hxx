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
 * Returns the number of jobs with the given plan which were started
 * recently.
 *
 * Throws on error.
 */
unsigned
PgCountRecentlyStartedJobs(Pg::Connection &db, const char *plan_name,
			   std::chrono::seconds duration);

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

#endif
