/*
 * SQL to C wrappers for queue.c.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef __QUEUE_PG_H
#define __QUEUE_PG_H

#include <postgresql/libpq-fe.h>

int pg_listen(PGconn *conn);

int pg_notify(PGconn *conn);

int pg_release_jobs(PGconn *conn, const char *node_name);

int pg_expire_jobs(PGconn *conn, const char *except_node_name);

int pg_next_scheduled_job(PGconn *conn, const char *plans_include,
                          long *span_r);

int pg_select_new_jobs(PGconn *conn,
                       const char *plans_include, const char *plans_exclude,
                       const char *plans_lowprio,
                       unsigned limit,
                       PGresult **res_r);

int pg_claim_job(PGconn *conn, const char *job_id, const char *node_name,
                 const char *timeout);

int pg_set_job_progress(PGconn *conn, const char *job_id, unsigned progress,
                        const char *timeout);

int pg_rollback_job(PGconn *conn, const char *id);

int pg_set_job_done(PGconn *conn, const char *id, int status);

#endif
