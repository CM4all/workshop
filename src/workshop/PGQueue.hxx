/*
 * SQL to C wrappers for queue.c.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef __QUEUE_PG_H
#define __QUEUE_PG_H

#include <stdbool.h>

class PgConnection;
class PgResult;

bool
pg_listen(PgConnection &db);

bool
pg_notify(PgConnection &db);

int pg_release_jobs(PgConnection &db, const char *node_name);

int
pg_expire_jobs(PgConnection &db, const char *except_node_name);

int
pg_next_scheduled_job(PgConnection &db, const char *plans_include,
                      long *span_r);

PgResult
pg_select_new_jobs(PgConnection &db,
                   const char *plans_include, const char *plans_exclude,
                   const char *plans_lowprio,
                   unsigned limit);

int
pg_claim_job(PgConnection &db, const char *job_id, const char *node_name,
             const char *timeout);

int pg_set_job_progress(PgConnection &db, const char *job_id, unsigned progress,
                        const char *timeout);

int pg_rollback_job(PgConnection &db, const char *id);

int pg_set_job_done(PgConnection &db, const char *id, int status);

#endif
