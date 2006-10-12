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
#include <time.h>

struct queue {
    char *node_name;
    PGconn *conn;
    int fd;
    struct poll *poll;
    int ready;
    time_t next_expire_check;

    int next_scheduled_valid;
    time_t next_scheduled;

    PGresult *result;
    int result_row, result_num;
};

static int queue_reconnect(struct queue *queue);

/** the poll() callback handler; this function handles notifies sent
    by the PostgreSQL server */
static void queue_callback(struct pollfd *pollfd, void *ctx) {
    struct queue *queue = (struct queue*)ctx;
    PGnotify *notify;

    (void)pollfd;

    assert(pollfd->fd == queue->fd);

    PQconsumeInput(queue->conn);

    if (PQstatus(queue->conn) != CONNECTION_OK) {
        log(2, "connection to PostgreSQL lost; trying to reconnect\n");
        queue_reconnect(queue);
        return;
    }

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
    int ret;

    queue = (struct queue*)calloc(1, sizeof(*queue));
    if (queue == NULL)
        return errno;

    queue->node_name = strdup(node_name);
    if (queue->node_name == NULL) {
        queue_close(&queue);
        return -1;
    }

    /* connect to PostgreSQL */

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

    /* release jobs which might be claimed by a former instance of
       us */

    ret = pg_release_jobs(queue->conn, queue->node_name);
    if (ret < 0) {
        queue_close(&queue);
        return -1;
    }

    if (ret > 0) {
        log(2, "released %d stale jobs\n", ret);
        pg_notify(queue->conn);
    }

    /* listen on notifications */

    ret = pg_listen(queue->conn);
    if (ret < 0) {
        queue_close(&queue);
        return -1;
    }

    /* poll on libpq file descriptor */

    queue->fd = PQsocket(queue->conn);
    queue->poll = p;
    poll_add(queue->poll, queue->fd, POLLIN, queue_callback, queue);

    queue->ready = 1;

    /* done */

    *queue_r = queue;
    return 0;
}

void queue_close(struct queue **queue_r) {
    struct queue *queue;

    assert(queue_r != NULL);
    assert(*queue_r != NULL);

    queue = *queue_r;
    *queue_r = NULL;

    queue_flush(queue);

    if (queue->poll != NULL)
        poll_remove(queue->poll, queue->fd);

    if (queue->conn != NULL)
        PQfinish(queue->conn);

    if (queue->node_name != NULL)
        free(queue->node_name);

    free(queue);
}

static int queue_reconnect(struct queue *queue) {
    int ret;

    /* unregister old socket */

    if (queue->fd >= 0)
        poll_remove(queue->poll, queue->fd);

    /* reconnect */

    queue_flush(queue);
    PQreset(queue->conn);

    if (PQstatus(queue->conn) != CONNECTION_OK) {
        queue->fd = -1;
        log(2, "reconnect to PostgreSQL failed: %s\n",
            PQerrorMessage(queue->conn));
        return -1;
    }

    /* listen on notifications */

    ret = pg_listen(queue->conn);
    if (ret < 0)
        log(1, "re-LISTEN failed\n");

    /* register new socket */

    queue->fd = PQsocket(queue->conn);
    poll_add(queue->poll, queue->fd, POLLIN, queue_callback, queue);

    /* reset some state variables */

    queue->next_scheduled_valid = 0;
    queue->ready = 1;

    return 0;
}

static int queue_autoreconnect(struct queue *queue) {
    if (PQstatus(queue->conn) == CONNECTION_OK)
        return 0;

    return queue_reconnect(queue);
}

int queue_next_scheduled(struct queue *queue, const char *plans_include,
                         int *span_r) {
    int ret;
    long span;

    if (queue->next_scheduled_valid) {
        if (queue->next_scheduled == 0) {
            *span_r = -1;
            return 0;
        } else {
            *span_r = (int)(queue->next_scheduled - time(NULL) + 2);
            if (*span_r < 0)
                *span_r = 0;
            return 1;
        }
    }

    ret = pg_next_scheduled_job(queue->conn, plans_include, &span);
    if (ret > 0 && span > 0) {
        if (span > 600)
            span = 600;

        *span_r = (int)span + 2;
        queue->next_scheduled = time(NULL) + span;
    } else {
        *span_r = -1;
        queue->next_scheduled = 0;
    }

    queue->next_scheduled = 1;

    return ret;
}

void queue_flush(struct queue *queue) {
    if (queue->result == NULL)
        return;

    PQclear(queue->result);
    queue->result = NULL;
    queue->result_row = 0;
    queue->result_num = 0;
}

int queue_fill(struct queue *queue, const char *plans_include,
               const char *plans_exclude) {
    int ret;
    time_t now;

    assert(queue->result == NULL);
    assert(queue->result_row == 0);
    assert(queue->result_num == 0);
    assert(plans_include != NULL && *plans_include == '{');
    assert(plans_exclude == NULL || *plans_exclude == '{');

    ret = queue_autoreconnect(queue);
    if (ret < 0)
        return -1;

    /* check expired jobs from all other nodes except us */

    now = time(NULL);
    if (now >= queue->next_expire_check) {
        queue->next_expire_check = now + 60;

        ret = pg_expire_jobs(queue->conn, queue->node_name);
        if (ret < 0)
            return -1;

        if (ret > 0) {
            log(2, "released %d expired jobs\n", ret);
            pg_notify(queue->conn);
        }
    }

    /* continue only if we got a "new_job" notify from PostgreSQL */

    if ((!queue->ready && (queue->next_scheduled == 0 ||
                           now < queue->next_scheduled)) ||
        strcmp(plans_include, "{}") == 0)
        return 0;

    queue->next_scheduled_valid = 0;
    queue->next_scheduled = 0;

    if (plans_exclude == NULL)
        plans_exclude = "{}";

    log(7, "requesting new jobs from database; plans_include=%s plans_exclude=%s\n",
        plans_include, plans_exclude);

    ret = pg_select_new_jobs(queue->conn, plans_include, plans_exclude,
                             &queue->result);
    if (ret <= 0) {
        if (strcmp(plans_exclude, "{}") == 0)
            queue->ready = 0;
        return ret;
    }

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
    if (queue->result == NULL ||
        queue->result_row >= PQntuples(queue->result))
        return 0;

    return get_next_job(queue, job_r);
}

int job_claim(struct job **job_r, const char *timeout) {
    struct job *job;
    int ret;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;

    log(6, "attempting to claim job %s\n", job->id);

    ret = pg_claim_job(job->queue->conn, job->id, job->queue->node_name,
                       timeout);
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

int job_set_progress(struct job *job, unsigned progress,
                     const char *timeout) {
    log(5, "job %s progress=%u\n", job->id, progress);

    return pg_set_job_progress(job->queue->conn, job->id, progress,
                               timeout);
}

int job_rollback(struct job **job_r) {
    struct job *job;
    int ret;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    ret = queue_autoreconnect(job->queue);
    if (ret < 0)
        return -1;

    log(6, "rolling back job %s\n", job->id);

    pg_rollback_job(job->queue->conn, job->id);

    free_job(&job);

    pg_notify(job->queue->conn);

    return 0;
}

int job_done(struct job **job_r, int status) {
    struct job *job;
    int ret;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    ret = queue_autoreconnect(job->queue);
    if (ret < 0)
        return -1;

    log(6, "job %s done with status %d\n", job->id, status);

    pg_set_job_done(job->queue->conn, job->id, status);

    free_job(&job);

    return 0;
}
