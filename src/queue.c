/*
 * $Id$
 *
 * Implementation of the job queue with PostgreSQL and async notifies.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "workshop.h"

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <poll.h>

#include <postgresql/libpq-fe.h>

struct queue {
    char *node_name;
    PGconn *conn;
    int fd;
    struct poll *poll;
    int ready;
    PGresult *result;
    int result_row, result_num;
};

/** the poll() callback handler; this function handles notifies sent
    by the PostgreSQL server */
static void queue_callback(struct pollfd *pollfd, void *ctx) {
    struct queue *queue = (struct queue*)ctx;
    PGnotify *notify;

    (void)pollfd;

    assert(pollfd->fd == queue->fd);

    PQconsumeInput(queue->conn);

    while ((notify = PQnotifies(queue->conn)) != NULL) {
        if (verbose >= 3)
            fprintf(stderr,
                    "ASYNC NOTIFY of '%s' received from backend pid %d\n",
                    notify->relname, notify->be_pid);
        if (strcmp(notify->relname, "new_job") == 0)
            queue->ready = 1;
        PQfreemem(notify);
    }
}

int queue_open(const char *node_name,
               const char *conninfo, struct poll *p,
               struct queue **queue_r) {
    struct queue *queue;
    PGresult *res;

    queue = (struct queue*)calloc(1, sizeof(*queue));
    if (queue == NULL)
        return errno;

    queue->node_name = strdup(node_name);
    if (queue->node_name == NULL) {
        queue_close(&queue);
        return -1;
    }

    queue->conn = PQconnectdb(conninfo);
    if (queue->conn == NULL) {
        queue_close(&queue);
        return ENOMEM;
    }

    if (PQstatus(queue->conn) != CONNECTION_OK) {
        fprintf(stderr, "failed to connect to postgresql: %s\n",
                PQerrorMessage(queue->conn));
        queue_close(&queue);
        return -1;
    }

    res = PQexec(queue->conn, "LISTEN new_job");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "LISTEN command failed: %s",
                PQerrorMessage(queue->conn));
        PQclear(res);
        queue_close(&queue);
        return -1;
    }

    PQclear(res);

    queue->fd = PQsocket(queue->conn);
    queue->poll = p;
    poll_add(queue->poll, queue->fd, POLLIN, queue_callback, queue);

    queue->ready = 1;

    *queue_r = queue;
    return 0;
}

void queue_close(struct queue **queue_r) {
    struct queue *queue;

    assert(queue_r != NULL);
    assert(*queue_r != NULL);

    queue = *queue_r;
    *queue_r = NULL;

    if (queue->poll != NULL)
        poll_remove(queue->poll, queue->fd);

    if (queue->conn != NULL)
        PQfinish(queue->conn);

    if (queue->node_name != NULL)
        free(queue->node_name);

    free(queue);
}

void queue_flush(struct queue *queue) {
    if (queue->result == NULL)
        return;

    PQclear(queue->result);
    queue->result = NULL;
    queue->result_row = 0;
    queue->result_num = 0;
}

/** query new jobs from the database */
static int fill_queue(struct queue *queue) {
    assert(queue->result == NULL);
    assert(queue->result_row == 0);
    assert(queue->result_num == 0);

    queue->result = PQexec(queue->conn, "SELECT id,plan_name,args,syslog_server "
                           "FROM jobs WHERE node_name IS NULL "
                           "ORDER BY priority,time_created LIMIT 1");
    if (PQresultStatus(queue->result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "SELECT on jobs failed: %s\n",
                PQerrorMessage(queue->conn));
        PQclear(queue->result);
        queue->result = NULL;
        return -1;
    }

    queue->result_num = PQntuples(queue->result);

    if (queue->result_num == 0) {
        PQclear(queue->result);
        queue->result = NULL;
        return 0;
    }

    return 1;
}

static char *my_strdup(const char *p) {
    if (p == NULL || *p == 0)
        return NULL;
    return strdup(p);
}

static void free_job(struct job **job_r) {
    struct job *job;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    if (job->id != NULL)
        free(job->id);
    if (job->plan_name != NULL)
        free(job->plan_name);
    if (job->syslog_server != NULL)
        free(job->syslog_server);
    if (job->args != NULL) {
        if (job->args[0] != NULL)
            free(job->args[0]);
        free(job->args);
    }

    free(job);
}

static int get_next_job(struct queue *queue, struct job **job_r) {
    struct job *job;
    PGresult *res;
    int row;

    assert(queue != NULL);
    assert(queue->result != NULL);
    assert(job_r != NULL);

    job = (struct job*)calloc(1, sizeof(*job));
    if (job == NULL)
        return -1;

    res = queue->result;
    row = queue->result_row++;

    assert(row < PQntuples(res));

    job->id = my_strdup(PQgetvalue(res, row, 0));
    job->plan_name = my_strdup(PQgetvalue(res, row, 1));
    /* XXX job->args = strdup(PQgetvalue(res, row, 2)); */
    job->syslog_server = my_strdup(PQgetvalue(res, row, 3));

    if (job->id == NULL || job->plan_name == NULL) {
        free_job(&job);
        return -1;
    }

    *job_r = job;
    return 1;
}

int queue_get(struct queue *queue, struct job **job_r) {
    int ret;

    if (queue->result != NULL) {
        if (queue->result_row < PQntuples(queue->result)) {
            return get_next_job(queue, job_r);
        } else {
            return 0;
        }
    }

    if (!queue->ready)
        return 0;

    ret = fill_queue(queue);
    if (ret <= 0)
        return -1;

    return get_next_job(queue, job_r);
}

int queue_claim(struct queue *queue, struct job **job_r) {
    const char *values[2];
    PGresult *res;
    int ret;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    values[0] = queue->node_name;
    values[1] = (*job_r)->id;

    res = PQexecParams(queue->conn,
                       "UPDATE jobs SET node_name=$1 WHERE id=$2 AND node_name IS NULL",
                       2, NULL, values, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "UPDATE/claim on jobs failed: %s\n",
                PQerrorMessage(queue->conn));
        PQclear(res);
        free_job(job_r);
        return -1;
    }

    ret = atoi(PQcmdTuples(res));
    PQclear(res);

    if (ret == 0)
        free_job(job_r);

    return ret;
}

void queue_skip(struct queue *queue, struct job **job_r) {
    (void)queue;

    free_job(job_r);
}

static int rollback_job(struct queue *queue, const char *id) {
    PGresult *res;
    int ret;

    res = PQexecParams(queue->conn,
                       "UPDATE jobs SET node_name=NULL, percent_done=0 WHERE id=$1",
                       1, NULL, &id, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "UPDATE/done on jobs failed: %s\n",
                PQerrorMessage(queue->conn));
        PQclear(res);
        return -1;
    }

    ret = atoi(PQcmdTuples(res));
    PQclear(res);
    return ret;
}

int queue_rollback(struct queue *queue, struct job **job_r) {
    struct job *job;

    (void)queue;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    rollback_job(queue, job->id);

    free_job(&job);

    return 0;
}

static int set_job_done(struct queue *queue, const char *id, int status) {
    char status_string[16];
    const char *params[2];
    PGresult *res;
    int ret;

    snprintf(status_string, sizeof(status_string), "%d", status);
    params[0] = id;
    params[1] = status_string;

    res = PQexecParams(queue->conn,
                       "UPDATE jobs SET time_done=NOW(), percent_done=100, status=$2 WHERE id=$1",
                       2, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "UPDATE/done on jobs failed: %s\n",
                PQerrorMessage(queue->conn));
        PQclear(res);
        return -1;
    }

    ret = atoi(PQcmdTuples(res));
    PQclear(res);
    return ret;
}

int queue_done(struct queue *queue, struct job **job_r, int status) {
    struct job *job;

    (void)queue;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    set_job_done(queue, job->id, status);

    free_job(&job);

    return 0;
}
