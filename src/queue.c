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

#include <event.h>

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

struct queue {
    char *node_name;
    PGconn *conn;
    int fd;
    int disabled, running, notified;

    /** if set to 1, the current queue run should be interrupted, to
        be started again */
    int interrupt;

    /** the next timeout is just a queue_run() restart */
    int again;

    struct event event;
    char *plans_include, *plans_exclude;
    time_t next_expire_check;

    queue_callback_t callback;
    void *ctx;
};

static int queue_reconnect(struct queue *queue);

static int queue_has_notify(const struct queue *queue);

/** the poll() callback handler; this function handles notifies sent
    by the PostgreSQL server */
static void queue_callback(int fd, short event, void *ctx) {
    struct queue *queue = (struct queue*)ctx;
    int ret;

    (void)fd;

    assert(fd == queue->fd);

    PQconsumeInput(queue->conn);

    if (PQstatus(queue->conn) != CONNECTION_OK) {
        log(2, "connection to PostgreSQL lost; trying to reconnect\n");
        ret = queue_reconnect(queue);
        if (ret == 0)
            queue_run(queue);
        return;
    }

    if (event == EV_TIMEOUT && !queue->again)
        log(7, "queue timeout\n");

    queue->again = 0;

    if (queue_has_notify(queue) || queue->notified || event == EV_TIMEOUT)
        queue_run(queue);
}

static void queue_set_timeout(struct queue *queue, struct timeval *tv) {
    event_del(&queue->event);
    event_set(&queue->event, queue->fd, EV_TIMEOUT|EV_READ|EV_PERSIST,
              queue_callback, queue);
    event_add(&queue->event, tv);
}

int queue_open(const char *node_name, const char *conninfo,
               queue_callback_t callback, void *ctx,
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
    event_set(&queue->event, queue->fd, EV_TIMEOUT|EV_READ|EV_PERSIST,
              queue_callback, queue);
    event_add(&queue->event, NULL);

    queue->callback = callback;
    queue->ctx = ctx;

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

    assert(!queue->running);

    if (queue->fd >= 0)
        event_del(&queue->event);

    if (queue->conn != NULL)
        PQfinish(queue->conn);

    if (queue->plans_include != NULL)
        free(queue->plans_include);

    if (queue->plans_exclude != NULL)
        free(queue->plans_exclude);

    if (queue->node_name != NULL)
        free(queue->node_name);

    free(queue);
}

static int queue_reconnect(struct queue *queue) {
    int ret;

    /* unregister old socket */

    if (queue->fd >= 0) {
        event_del(&queue->event);
        queue->fd = -1;
    }

    /* reconnect */

    PQreset(queue->conn);

    if (PQstatus(queue->conn) != CONNECTION_OK) {
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
    event_set(&queue->event, queue->fd, EV_READ|EV_PERSIST, queue_callback, queue);
    event_add(&queue->event, NULL);

    return 0;
}

static int queue_autoreconnect(struct queue *queue) {
    if (PQstatus(queue->conn) == CONNECTION_OK)
        return 0;

    return queue_reconnect(queue);
}

static int queue_has_notify(const struct queue *queue) {
    PGnotify *notify;
    int ret = 0;

    if (queue->conn == NULL)
        return 0;

    while ((notify = PQnotifies(queue->conn)) != NULL) {
        log(6, "async notify '%s' received from backend pid %d\n",
            notify->relname, notify->be_pid);
        if (strcmp(notify->relname, "new_job") == 0)
            ret = 1;
        PQfreemem(notify);
    }

    return ret;
}

static void queue_check_notify(struct queue *queue) {
    struct timeval tv;

    if (!queue_has_notify(queue))
        return;

    queue->notified = 1;

    /* there are pending notifies - set a very short timeout, so
       libevent will call us very soon */

    tv.tv_sec = 0;
    tv.tv_usec = 10000;
    queue_set_timeout(queue, &tv);
}

static int queue_next_scheduled(struct queue *queue, int *span_r) {
    int ret;
    long span;

    if (queue->plans_include == NULL) {
        *span_r = -1;
        return 0;
    }

    ret = pg_next_scheduled_job(queue->conn, queue->plans_include, &span);
    if (ret > 0) {
        if (span < 0)
            span = 0;

        /* try to avoid rounding errors: always add 2 seconds */
        span += 2;

        if (span > 600)
            span = 600;

        *span_r = (int)span;
    } else {
        *span_r = -1;
    }

    return ret;
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

static int get_job(struct queue *queue, PGresult *res, int row,
                   struct job **job_r) {
    struct job *job;
    int ret;

    assert(queue != NULL);
    assert(job_r != NULL);

    job = (struct job*)calloc(1, sizeof(*job));
    if (job == NULL)
        return -1;

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

    queue_check_notify(queue);

    *job_r = job;
    return 1;
}

static int get_and_claim_job(struct queue *queue, PGresult *res, int row,
                             const char *timeout, struct job **job_r) {
    int ret;
    struct job *job;

    ret = get_job(queue, res, row, &job);
    if (ret <= 0)
        return ret;

    log(6, "attempting to claim job %s\n", job->id);

    ret = pg_claim_job(queue->conn, job->id, queue->node_name,
                       timeout);
    if (ret < 0) {
        free_job(&job);
        return -1;
    }

    if (ret == 0) {
        log(6, "job %s was not claimed\n", job->id);
        free_job(&job);
        return 0;
    }

    log(6, "job %s claimed\n", job->id);

    queue_check_notify(queue);

    *job_r = job;
    return 1;
}

static int copy_string(char **dest_r, const char *src) {
    assert(dest_r != NULL);
    assert(src != NULL);

    if (*dest_r != NULL) {
        if (strcmp(*dest_r, src) == 0)
            return 0;

        free(*dest_r);
    }

    *dest_r = strdup(src);
    if (*dest_r == NULL)
        abort();
    return 1;
}

void queue_set_filter(struct queue *queue, const char *plans_include,
                      const char *plans_exclude) {
    int r1, r2;

    r1 = copy_string(&queue->plans_include, plans_include);
    r2 = copy_string(&queue->plans_exclude, plans_exclude);

    if (r1 || r2)
        queue_run(queue);
}

static int queue_run2(struct queue *queue) {
    PGresult *result;
    int ret, row, num;
    struct job *job;
    time_t now;
    struct timeval tv;

    if (queue->plans_include == NULL ||
        strcmp(queue->plans_include, "{}") == 0 ||
        queue->plans_exclude == NULL)
        return 0;

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

    /* query database */

    queue->interrupt = 0;

    log(7, "requesting new jobs from database; plans_include=%s plans_exclude=%s\n",
        queue->plans_include, queue->plans_exclude);

    num = pg_select_new_jobs(queue->conn, queue->plans_include, queue->plans_exclude,
                             &result);
    if (num > 0) {
        for (row = 0; row < num && !queue->disabled && !queue->interrupt; ++row) {
            ret = get_and_claim_job(queue, result, row, "5 minutes", &job);
            if (ret < 0)
                break;

            if (ret == 0)
                continue;

            queue->callback(job, queue->ctx);
        }

        PQclear(result);
    }

    if (queue->disabled) {
        log(7, "queue has been disabled\n");
        return num;
    }

    /* update timeout */

    if (queue->interrupt) {
        /* we have been interrupted: run again in 100ms */
        log(7, "aborting queue run\n");

        queue->again = 1;
        tv.tv_sec = 0;
        tv.tv_usec = 10000;
    } else {
        queue_next_scheduled(queue, &ret);
        if (ret >= 0)
            log(3, "next scheduled job is in %d seconds\n", ret);
        else
            ret = 600;

        tv.tv_sec = ret;
        tv.tv_usec = 0;
    }

    queue_set_timeout(queue, &tv);

    return num;
}

int queue_run(struct queue *queue) {
    int ret;

    queue->interrupt = 1;

    if (queue->running || queue->disabled)
        return 0;

    queue->running = 1;
    ret = queue_run2(queue);
    queue->notified = 0;
    queue->running = 0;

    queue_check_notify(queue);

    return ret;
}

void queue_disable(struct queue *queue) {
    if (queue->disabled)
        return;

    queue->disabled = 1;
}

void queue_enable(struct queue *queue) {
    if (!queue->disabled)
        return;

    queue->disabled = 0;
    queue_run(queue);
}

int job_set_progress(struct job *job, unsigned progress,
                     const char *timeout) {
    int ret;

    log(5, "job %s progress=%u\n", job->id, progress);

    ret = pg_set_job_progress(job->queue->conn, job->id, progress,
                              timeout);

    queue_check_notify(job->queue);

    return ret;
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

    pg_notify(job->queue->conn);

    queue_check_notify(job->queue);

    free_job(&job);

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

    queue_check_notify(job->queue);

    free_job(&job);

    return 0;
}
