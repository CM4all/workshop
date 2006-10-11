/*
 * $Id$
 *
 * SQL to C wrappers for queue.c.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "pg-queue.h"

#include <assert.h>
#include <stdlib.h>

int pg_listen(PGconn *conn) {
    PGresult *res;

    res = PQexec(conn, "LISTEN new_job");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "LISTEN new_job failed: %s\n",
                PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    PQclear(res);
    return 0;
}

int pg_notify(PGconn *conn) {
    PGresult *res;

    res = PQexec(conn, "NOTIFY new_job");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "NOTIFY new_job failed: %s\n",
                PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    PQclear(res);
    return 0;
}

int pg_release_jobs(PGconn *conn, const char *node_name) {
    PGresult *res;
    int ret;

    res = PQexecParams(conn,
                       "UPDATE jobs SET node_name=NULL, node_timeout=NULL, progress=0 "
                       "WHERE node_name=$1 AND time_done IS NULL AND exit_status IS NULL",
                       1, NULL, &node_name, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "UPDATE/claim on jobs failed: %s\n",
                PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    ret = atoi(PQcmdTuples(res));
    PQclear(res);
    return ret;
}

int pg_expire_jobs(PGconn *conn, const char *except_node_name) {
    PGresult *res;
    int ret;

    res = PQexecParams(conn,
                       "UPDATE jobs SET node_name=NULL, node_timeout=NULL, progress=0 "
                       "WHERE time_done IS NULL AND "
                       "node_name IS NOT NULL AND node_name <> $1 AND "
                       "node_timeout IS NOT NULL AND NOW() > node_timeout",
                       1, NULL, &except_node_name, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "UPDATE/expire on jobs failed: %s\n",
                PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    ret = atoi(PQcmdTuples(res));
    PQclear(res);
    return ret;
}

int pg_select_new_jobs(PGconn *conn, PGresult **res_r) {
    PGresult *res;
    int ret;

    assert(res_r != NULL);
    assert(*res_r == NULL);

    res = PQexec(conn, "SELECT id,plan_name,args,syslog_server "
                 "FROM jobs WHERE node_name IS NULL AND exit_status IS NULL "
                 "ORDER BY priority,time_created");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "SELECT on jobs failed: %s\n",
                PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    ret = PQntuples(res);
    if (ret == 0) {
        PQclear(res);
        return 0;
    }

    *res_r = res;
    return ret;
}

int pg_claim_job(PGconn *conn, const char *job_id, const char *node_name,
                 const char *timeout) {
    const char *values[3];
    PGresult *res;
    int ret;

    values[0] = node_name;
    values[1] = job_id;
    values[2] = timeout;

    res = PQexecParams(conn,
                       "UPDATE jobs SET node_name=$1, node_timeout=NOW()+$3::INTERVAL "
                       "WHERE id=$2 AND node_name IS NULL",
                       3, NULL, values, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "UPDATE/claim on jobs failed: %s\n",
                PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    ret = atoi(PQcmdTuples(res));
    PQclear(res);
    return ret;
}

int pg_set_job_progress(PGconn *conn, const char *job_id, unsigned progress,
                        const char *timeout) {
    char progress_s[32];
    const char *params[3];
    PGresult *res;
    int ret;

    snprintf(progress_s, sizeof(progress_s), "%u", progress);
    params[0] = job_id;
    params[1] = progress_s;
    params[2] = timeout;

    res = PQexecParams(conn,
                       "UPDATE jobs SET progress=$2, node_timeout=NOW()+$3::INTERVAL WHERE id=$1",
                       3, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "UPDATE/progress on jobs failed: %s\n",
                PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    ret = atoi(PQcmdTuples(res));
    PQclear(res);
    return ret;
}

int pg_rollback_job(PGconn *conn, const char *id) {
    PGresult *res;
    int ret;

    res = PQexecParams(conn,
                       "UPDATE jobs SET node_name=NULL, node_timeout=NULL, progress=0 "
                       "WHERE id=$1 AND node_name IS NOT NULL AND time_done IS NULL",
                       1, NULL, &id, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "UPDATE/done on jobs failed: %s\n",
                PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    ret = atoi(PQcmdTuples(res));
    PQclear(res);
    return ret;
}

int pg_set_job_done(PGconn *conn, const char *id, int status) {
    char status_string[16];
    const char *params[2];
    PGresult *res;
    int ret;

    snprintf(status_string, sizeof(status_string), "%d", status);
    params[0] = id;
    params[1] = status_string;

    res = PQexecParams(conn,
                       "UPDATE jobs SET time_done=NOW(), progress=100, exit_status=$2 WHERE id=$1",
                       2, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "UPDATE/done on jobs failed: %s\n",
                PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    ret = atoi(PQcmdTuples(res));
    PQclear(res);
    return ret;
}
