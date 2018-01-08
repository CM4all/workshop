/*
 * SQL to C wrappers for queue.c.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef __QUEUE_PG_H
#define __QUEUE_PG_H

#include <chrono>

namespace Pg {
class Connection;
class Result;
}

bool
pg_notify(Pg::Connection &db);

int pg_release_jobs(Pg::Connection &db, const char *node_name);

int
pg_expire_jobs(Pg::Connection &db, const char *except_node_name);

int
pg_next_scheduled_job(Pg::Connection &db,
                      const char *plans_include,
                      long *span_r);

Pg::Result
pg_select_new_jobs(Pg::Connection &db,
                   const char *plans_include, const char *plans_exclude,
                   const char *plans_lowprio,
                   unsigned limit);

int
pg_claim_job(Pg::Connection &db,
             const char *job_id, const char *node_name,
             const char *timeout);

/**
 * Throws on error.
 */
void
pg_set_job_progress(Pg::Connection &db, const char *job_id, unsigned progress,
                    const char *timeout);

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
pg_again_job(Pg::Connection &db, const char *id, std::chrono::seconds delay);

/**
 * Throws on error.
 */
void
pg_set_job_done(Pg::Connection &db, const char *id, int status,
                const char *log);

#endif
