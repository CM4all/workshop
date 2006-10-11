/*
 * $Id$
 *
 * SQL to C wrappers for queue.c.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef __QUEUE_PG_H
#define __QUEUE_PG_H

#include <postgresql/libpq-fe.h>

int pg_select_new_jobs(PGconn *conn, PGresult **res_r);

int pg_claim_job(PGconn *conn, const char *job_id, const char *node_name);

int pg_set_job_progress(PGconn *conn, const char *job_id, unsigned progress);

int pg_rollback_job(PGconn *conn, const char *id);

int pg_set_job_done(PGconn *conn, const char *id, int status);

#endif
