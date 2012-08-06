/*
 * SQL to C wrappers for queue.c.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef __QUEUE_PG_H
#define __QUEUE_PG_H

#include <stdbool.h>

class DatabaseConnection;
class DatabaseResult;

bool
pg_listen(DatabaseConnection &db);

bool
pg_notify(DatabaseConnection &db);

int pg_release_jobs(DatabaseConnection &db, const char *node_name);

int
pg_expire_jobs(DatabaseConnection &db, const char *except_node_name);

int
pg_next_scheduled_job(DatabaseConnection &db, const char *plans_include,
                      long *span_r);

DatabaseResult
pg_select_new_jobs(DatabaseConnection &db,
                   const char *plans_include, const char *plans_exclude,
                   const char *plans_lowprio,
                   unsigned limit);

int
pg_claim_job(DatabaseConnection &db, const char *job_id, const char *node_name,
             const char *timeout);

int pg_set_job_progress(DatabaseConnection &db, const char *job_id, unsigned progress,
                        const char *timeout);

int pg_rollback_job(DatabaseConnection &db, const char *id);

int pg_set_job_done(DatabaseConnection &db, const char *id, int status);

#endif
