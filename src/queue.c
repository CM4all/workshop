/*
 * Implementation of the job queue with PostgreSQL and async notifies.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "queue.h"
#include "pg-util.h"
#include "pg-queue.h"

#include <daemon/log.h>

#include <glib.h>
#include <event.h>

#include <sys/types.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

struct queue {
    char *node_name;
    PGconn *conn;
    int fd;
    int disabled, running;

    /** if set to 1, the current queue run should be interrupted, to
        be started again */
    int interrupt;

    /**
     * For detecting notifies from PostgreSQL.
     */
    struct event read_event;

    /**
     * Timer event for which runs the queue or reconnects to
     * PostgreSQL.
     */
    struct event timer_event;

    char *plans_include, *plans_exclude, *plans_lowprio;
    time_t next_expire_check;

    queue_callback_t callback;
    void *ctx;
};

static int queue_autoreconnect(struct queue *queue);

static int queue_has_notify(const struct queue *queue);

static void
queue_run(struct queue *queue);

/** the poll() callback handler; this function handles notifies sent
    by the PostgreSQL server */
static void
queue_event_callback(G_GNUC_UNUSED int fd, G_GNUC_UNUSED short event,
                     void *ctx)
{
    struct queue *queue = (struct queue*)ctx;
    int ret;

    assert(fd == queue->fd);
    assert(!queue->running);

    PQconsumeInput(queue->conn);

    ret = queue_autoreconnect(queue);
    if (ret != 0)
        return;

    if (queue_has_notify(queue))
        queue_run(queue);

    assert(!queue->running);
}

static void
queue_timer_event_callback(G_GNUC_UNUSED int fd, G_GNUC_UNUSED short event,
                           void *ctx)
{
    struct queue *queue = (struct queue *)ctx;
    int ret;

    assert(!queue->running);

    ret = queue_autoreconnect(queue);
    if (ret != 0)
        return;

    queue_run(queue);

    assert(!queue->running);
}

static void queue_set_timeout(struct queue *queue, struct timeval *tv) {
    assert(tv != NULL);

    evtimer_del(&queue->timer_event);
    evtimer_add(&queue->timer_event, tv);
}

void
queue_reschedule(struct queue *queue)
{
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 10000;

    queue_set_timeout(queue, &tv);
}

int queue_open(const char *node_name, const char *conninfo,
               queue_callback_t callback, void *ctx,
               struct queue **queue_r) {
    struct queue *queue;
    int ret;

    queue = g_new0(struct queue, 0);
    queue->node_name = g_strdup(node_name);

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
        daemon_log(2, "released %d stale jobs\n", ret);
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
    event_set(&queue->read_event, queue->fd, EV_READ|EV_PERSIST,
              queue_event_callback, queue);
    event_add(&queue->read_event, NULL);

    evtimer_set(&queue->timer_event, queue_timer_event_callback, queue);

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
        event_del(&queue->read_event);

    evtimer_del(&queue->timer_event);

    if (queue->conn != NULL)
        PQfinish(queue->conn);

    g_free(queue->plans_include);
    g_free(queue->plans_exclude);
    g_free(queue->plans_lowprio);
    g_free(queue->node_name);
    g_free(queue);
}

static int queue_reconnect(struct queue *queue) {
    int ret;

    /* unregister old socket */

    if (queue->fd >= 0) {
        event_del(&queue->read_event);
        queue->fd = -1;
    }

    /* reconnect */

    PQreset(queue->conn);

    if (PQstatus(queue->conn) != CONNECTION_OK) {
        struct timeval tv;

        daemon_log(2, "reconnect to PostgreSQL failed: %s\n",
                   PQerrorMessage(queue->conn));

        tv.tv_sec = 10;
        tv.tv_usec = 0;
        queue_set_timeout(queue, &tv);

        return -1;
    }

    /* listen on notifications */

    ret = pg_listen(queue->conn);
    if (ret < 0)
        daemon_log(1, "re-LISTEN failed\n");

    /* register new socket */

    queue->fd = PQsocket(queue->conn);
    event_set(&queue->read_event, queue->fd, EV_READ|EV_PERSIST,
              queue_event_callback, queue);
    event_add(&queue->read_event, NULL);

    queue_reschedule(queue);

    return 1;
}

static int queue_autoreconnect(struct queue *queue) {
    if (PQstatus(queue->conn) == CONNECTION_OK)
        return 0;

    if (queue->fd < 0)
        daemon_log(2, "re-trying to reconnect to PostgreSQL\n");
    else
        daemon_log(2, "connection to PostgreSQL lost; trying to reconnect\n");

    return queue_reconnect(queue);
}

static int queue_has_notify(const struct queue *queue) {
    PGnotify *notify;
    int ret = 0;

    if (queue->conn == NULL)
        return 0;

    while ((notify = PQnotifies(queue->conn)) != NULL) {
        daemon_log(6, "async notify '%s' received from backend pid %d\n",
                   notify->relname, notify->be_pid);
        if (strcmp(notify->relname, "new_job") == 0)
            ret = 1;
        PQfreemem(notify);
    }

    return ret;
}

static void queue_check_notify(struct queue *queue) {
    if (queue_has_notify(queue))
        /* there are pending notifies - set a very short timeout, so
           libevent will call us very soon */
        queue_reschedule(queue);
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
    return g_strdup(p);
}

static void free_job(struct job **job_r) {
    struct job *job;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    g_free(job->id);
    g_free(job->plan_name);
    g_free(job->syslog_server);

    strarray_free(&job->args);

    g_free(job);
}

static int get_job(struct queue *queue, PGresult *res, int row,
                   struct job **job_r) {
    struct job *job;
    int ret;

    assert(queue != NULL);
    assert(job_r != NULL);

    job = g_new0(struct job, 1);
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

    daemon_log(6, "attempting to claim job %s\n", job->id);

    ret = pg_claim_job(queue->conn, job->id, queue->node_name,
                       timeout);
    if (ret < 0) {
        free_job(&job);
        return -1;
    }

    if (ret == 0) {
        daemon_log(6, "job %s was not claimed\n", job->id);
        free_job(&job);
        return 0;
    }

    daemon_log(6, "job %s claimed\n", job->id);

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

        g_free(*dest_r);
    }

    *dest_r = g_strdup(src);
    return 1;
}

void queue_set_filter(struct queue *queue, const char *plans_include,
                      const char *plans_exclude,
                      const char *plans_lowprio) {
    int r1, r2;

    r1 = copy_string(&queue->plans_include, plans_include);
    r2 = copy_string(&queue->plans_exclude, plans_exclude);
    copy_string(&queue->plans_lowprio, plans_lowprio);

    if (r1 || r2) {
        if (queue->running)
            queue->interrupt = 1;
        else if (queue->fd >= 0)
            queue_run(queue);
    }
}

static void
queue_run_result(struct queue *queue, int num, PGresult *result)
{
    int row, ret;
    struct job *job;

    for (row = 0; row < num && !queue->disabled && !queue->interrupt; ++row) {
        ret = get_and_claim_job(queue, result, row, "5 minutes", &job);
        if (ret > 0)
            queue->callback(job, queue->ctx);
        else if (ret < 0)
            break;
    }
}

static void
queue_run2(struct queue *queue)
{
    PGresult *result;
    int ret, num, full = 0;
    time_t now;

    assert(!queue->disabled);
    assert(queue->running);
    assert(!queue->fd >= 0);

    if (queue->plans_include == NULL ||
        strcmp(queue->plans_include, "{}") == 0 ||
        queue->plans_exclude == NULL)
        return;

    /* check expired jobs from all other nodes except us */

    now = time(NULL);
    if (now >= queue->next_expire_check) {
        queue->next_expire_check = now + 60;

        ret = pg_expire_jobs(queue->conn, queue->node_name);
        if (ret < 0)
            return;

        if (ret > 0) {
            daemon_log(2, "released %d expired jobs\n", ret);
            pg_notify(queue->conn);
        }
    }

    /* query database */

    queue->interrupt = 0;

    daemon_log(7, "requesting new jobs from database; plans_include=%s plans_exclude=%s plans_lowprio=%s\n",
               queue->plans_include, queue->plans_exclude, queue->plans_lowprio);

    num = pg_select_new_jobs(queue->conn,
                             queue->plans_include,
                             queue->plans_exclude,
                             queue->plans_lowprio,
                             16,
                             &result);
    if (num > 0) {
        queue_run_result(queue, num, result);
        PQclear(result);

        if (num == 16)
            full = 1;
    }

    if (!queue->disabled && !queue->interrupt &&
        strcmp(queue->plans_lowprio, "{}") != 0) {
        /* now also select plans which are already running */

        daemon_log(7, "requesting new jobs from database II; plans_lowprio=%s\n",
                   queue->plans_lowprio);

        num = pg_select_new_jobs(queue->conn,
                                 queue->plans_lowprio,
                                 queue->plans_exclude,
                                 "{}",
                                 16,
                                 &result);
        if (num > 0) {
            queue_run_result(queue, num, result);
            PQclear(result);

            if (num == 16)
                full = 1;
        }
    }

    /* update timeout */

    if (queue->disabled) {
        daemon_log(7, "queue has been disabled\n");
    } else if (queue->interrupt) {
        /* we have been interrupted: run again in 100ms */
        daemon_log(7, "aborting queue run\n");

        queue_reschedule(queue);
    } else if (full) {
        /* 16 is our row limit, and exactly 16 rows were returned - we
           suspect there may be more.  schedule next queue run in 1
           second */
        queue_reschedule(queue);
    } else {
        struct timeval tv;

        queue_next_scheduled(queue, &ret);
        if (ret >= 0)
            daemon_log(3, "next scheduled job is in %d seconds\n", ret);
        else
            ret = 600;

        tv.tv_sec = ret;
        tv.tv_usec = 0;
        queue_set_timeout(queue, &tv);
    }
}

static void
queue_run(struct queue *queue)
{
    assert(!queue->running);
    assert(!queue->fd >= 0);

    if (queue->disabled)
        return;

    queue->running = 1;
    queue_run2(queue);
    queue->running = 0;

    queue_check_notify(queue);
}

void queue_disable(struct queue *queue) {
    if (queue->disabled)
        return;

    queue->disabled = 1;
}

void queue_enable(struct queue *queue) {
    assert(!queue->running);

    if (!queue->disabled)
        return;

    queue->disabled = 0;

    if (queue->fd >= 0)
        queue_run(queue);
}

int job_set_progress(struct job *job, unsigned progress,
                     const char *timeout) {
    int ret;

    daemon_log(5, "job %s progress=%u\n", job->id, progress);

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

    daemon_log(6, "rolling back job %s\n", job->id);

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

    daemon_log(6, "job %s done with status %d\n", job->id, status);

    pg_set_job_done(job->queue->conn, job->id, status);

    queue_check_notify(job->queue);

    free_job(&job);

    return 0;
}
