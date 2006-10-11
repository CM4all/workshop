/*
 * $Id$
 *
 * Implementation of the job queue with PostgreSQL and async notifies.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "workshop.h"
#include "pg-util.h"
#include "pg-queue.h"

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <poll.h>

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
        log(6, "async notify '%s' received from backend pid %d\n",
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
    int ret;

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

    ret = pg_release_jobs(queue->conn, queue->node_name);
    if (ret < 0) {
        queue_close(&queue);
        return -1;
    }

    if (ret > 0)
        log(3, "released %d stale jobs\n", ret);

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
    int ret;

    assert(queue->result == NULL);
    assert(queue->result_row == 0);
    assert(queue->result_num == 0);

    ret = pg_select_new_jobs(queue->conn, &queue->result);
    if (ret <= 0)
        return ret;

    return queue->result_num = ret;
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

    strarray_free(&job->args);

    free(job);
}

static int get_next_job(struct queue *queue, struct job **job_r) {
    struct job *job;
    PGresult *res;
    int row, ret;

    assert(queue != NULL);
    assert(queue->result != NULL);
    assert(job_r != NULL);

    job = (struct job*)calloc(1, sizeof(*job));
    if (job == NULL)
        return -1;

    res = queue->result;
    row = queue->result_row++;

    assert(row < PQntuples(res));

    job->queue = queue;
    job->id = my_strdup(PQgetvalue(res, row, 0));
    job->plan_name = my_strdup(PQgetvalue(res, row, 1));

    ret = pg_decode_array(PQgetvalue(res, row, 2), &job->args);
    if (ret != 0) {
        fprintf(stderr, "pg_decode_array() failed\n");
        free_job(&job);
        return -1;
    }

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

int job_claim(struct job **job_r) {
    struct job *job;
    int ret;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;

    log(6, "attempting to claim job %s\n", job->id);

    ret = pg_claim_job(job->queue->conn, job->id, job->queue->node_name);
    if (ret < 0) {
        free_job(job_r);
        return -1;
    }

    if (ret == 0) {
        log(6, "job %s was not claimed\n", job->id);
        free_job(job_r);
        return 0;
    }

    log(6, "job %s claimed\n", job->id);

    return ret;
}

void job_skip(struct job **job_r) {
    free_job(job_r);
}

int job_set_progress(struct job *job, unsigned progress) {
    log(5, "job %s progress=%u\n", job->id, progress);

    return pg_set_job_progress(job->queue->conn, job->id, progress);
}

int job_rollback(struct job **job_r) {
    struct job *job;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    log(6, "rolling back job %s\n", job->id);

    pg_rollback_job(job->queue->conn, job->id);

    free_job(&job);

    return 0;
}

int job_done(struct job **job_r, int status) {
    struct job *job;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    log(6, "job %s done with status %d\n", job->id, status);

    pg_set_job_done(job->queue->conn, job->id, status);

    free_job(&job);

    return 0;
}
