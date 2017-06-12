/*
 * SQL to C wrappers for queue.c.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef __QUEUE_PG_H
#define __QUEUE_PG_H

namespace Pg {
class Connection;
class Result;
}

bool
pg_listen(Pg::Connection &db);

bool
pg_notify(Pg::Connection &db);

int pg_release_jobs(Pg::Connection &db, const char *node_name);

int
pg_expire_jobs(Pg::Connection &db, const char *except_node_name);

int
pg_next_scheduled_job(Pg::Connection &db, const char *plans_include,
                      long *span_r);

Pg::Result
pg_select_new_jobs(Pg::Connection &db,
                   const char *plans_include, const char *plans_exclude,
                   const char *plans_lowprio,
                   unsigned limit);

int
pg_claim_job(Pg::Connection &db, const char *job_id, const char *node_name,
             const char *timeout);

int pg_set_job_progress(Pg::Connection &db, const char *job_id, unsigned progress,
                        const char *timeout);

int pg_rollback_job(Pg::Connection &db, const char *id);

int
pg_set_job_done(Pg::Connection &db, const char *id, int status,
                const char *log);

#endif
