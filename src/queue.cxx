/*
 * Implementation of the job queue with PostgreSQL and async notifies.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "queue.hxx"
#include "job.hxx"
#include "pg_array.hxx"

extern "C" {
#include "pg-queue.h"
}

#include <inline/compiler.h>
#include <daemon/log.h>

#include <stdbool.h>
#include <sys/types.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

Queue::~Queue()
{
    assert(!running);

    if (fd >= 0)
        event_del(&read_event);

    evtimer_del(&timer_event);

    if (conn != NULL)
        PQfinish(conn);
}

static bool
queue_autoreconnect(Queue *queue);

static bool
queue_has_notify(const Queue *queue);

static void
queue_run(Queue *queue);

/** the poll() callback handler; this function handles notifies sent
    by the PostgreSQL server */
static void
queue_event_callback(gcc_unused int fd, gcc_unused short event,
                     void *ctx)
{
    Queue *queue = (Queue *)ctx;

    assert(fd == queue->fd);
    assert(!queue->running);

    PQconsumeInput(queue->conn);

    if (!queue_autoreconnect(queue))
        return;

    if (queue_has_notify(queue))
        queue_run(queue);

    assert(!queue->running);
}

static void
queue_timer_event_callback(gcc_unused int fd, gcc_unused short event,
                           void *ctx)
{
    Queue *queue = (Queue *)ctx;

    assert(!queue->running);

    if (!queue_autoreconnect(queue))
        return;

    queue_run(queue);

    assert(!queue->running);
}

static void queue_set_timeout(Queue *queue, struct timeval *tv) {
    assert(tv != NULL);

    evtimer_del(&queue->timer_event);
    evtimer_add(&queue->timer_event, tv);
}

void
queue_reschedule(Queue *queue)
{
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 10000;

    queue_set_timeout(queue, &tv);
}

int queue_open(const char *node_name, const char *conninfo,
               queue_callback_t callback, void *ctx,
               Queue **queue_r) {
    int ret;

    Queue *queue = new Queue(node_name, callback, ctx);

    evtimer_set(&queue->timer_event, queue_timer_event_callback, queue);

    /* connect to PostgreSQL */

    queue->conn = PQconnectdb(conninfo);
    if (queue->conn == NULL) {
        delete queue;
        return ENOMEM;
    }

    if (PQstatus(queue->conn) != CONNECTION_OK) {
        struct timeval tv;

        daemon_log(2, "connect to PostgreSQL failed: %s\n",
                   PQerrorMessage(queue->conn));

        tv.tv_sec = 10;
        tv.tv_usec = 0;
        queue_set_timeout(queue, &tv);

        *queue_r = queue;
        return 0;

    }

    /* release jobs which might be claimed by a former instance of
       us */

    ret = pg_release_jobs(queue->conn, queue->node_name.c_str());
    if (ret < 0) {
        delete queue;
        return -1;
    }

    if (ret > 0) {
        daemon_log(2, "released %d stale jobs\n", ret);
        pg_notify(queue->conn);
    }

    /* listen on notifications */

    ret = pg_listen(queue->conn);
    if (ret < 0) {
        delete queue;
        return -1;
    }

    /* poll on libpq file descriptor */

    queue->fd = PQsocket(queue->conn);
    event_set(&queue->read_event, queue->fd, EV_READ|EV_PERSIST,
              queue_event_callback, queue);
    event_add(&queue->read_event, NULL);

    /* done */

    *queue_r = queue;
    return 0;
}

/**
 * Reconnect to the database (unconditionally).
 *
 * @return true on success, false if a connection could not be
 * established
 */
static bool
queue_reconnect(Queue *queue)
{
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

        return false;
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

    return true;
}

/**
 * Check the status of the database connection, and reconnect when it
 * has gone bad.
 */
static bool
queue_autoreconnect(Queue *queue)
{
    if (PQstatus(queue->conn) == CONNECTION_OK)
        return true;

    if (queue->fd < 0)
        daemon_log(2, "re-trying to reconnect to PostgreSQL\n");
    else
        daemon_log(2, "connection to PostgreSQL lost; trying to reconnect\n");

    return queue_reconnect(queue);
}

static bool
queue_has_notify(const Queue *queue) {
    PGnotify *notify;
    bool ret = false;

    while ((notify = PQnotifies(queue->conn)) != NULL) {
        daemon_log(6, "async notify '%s' received from backend pid %d\n",
                   notify->relname, notify->be_pid);
        if (strcmp(notify->relname, "new_job") == 0)
            ret = true;
        PQfreemem(notify);
    }

    return ret;
}

static void queue_check_notify(Queue *queue) {
    if (queue_has_notify(queue))
        /* there are pending notifies - set a very short timeout, so
           libevent will call us very soon */
        queue_reschedule(queue);
}

/**
 * Checks everything asynchronously: if the connection has failed,
 * schedule a reconnect.  If there are notifies, schedule a queue run.
 *
 * This is an extended version of queue_check_notify(), to be used by
 * public functions that (unlike the internal functions) do not
 * reschedule.
 */
static void
queue_check_all(Queue *queue)
{
    if (queue_has_notify(queue) || PQstatus(queue->conn) != CONNECTION_OK)
        /* something needs to be done - schedule it for the timer
           event callback */
        queue_reschedule(queue);
}

static int queue_next_scheduled(Queue *queue, int *span_r) {
    int ret;
    long span;

    if (queue->plans_include.empty()) {
        *span_r = -1;
        return 0;
    }

    ret = pg_next_scheduled_job(queue->conn, queue->plans_include.c_str(),
                                &span);
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

static bool
get_job(Queue *queue, PGresult *res, int row,
        Job **job_r)
{
    Job *job;

    assert(queue != NULL);
    assert(job_r != NULL);
    assert(row < PQntuples(res));

    job = new Job(queue, PQgetvalue(res, row, 0), PQgetvalue(res, row, 1));

    std::list<std::string> args;
    if (!pg_decode_array(PQgetvalue(res, row, 2), args)) {
        fprintf(stderr, "pg_decode_array() failed\n");
        delete job;
        return false;
    }

    job->args.splice(job->args.end(), args, args.begin(), args.end());

    if (!PQgetisnull(res, row, 3))
        job->syslog_server = PQgetvalue(res, row, 3);

    if (job->id.empty() || job->plan_name.empty()) {
        delete job;
        return false;
    }

    *job_r = job;
    return true;
}

static int get_and_claim_job(Queue *queue, PGresult *res, int row,
                             const char *timeout, Job **job_r) {
    int ret;
    Job *job;

    if (!get_job(queue, res, row, &job))
        return -1;

    daemon_log(6, "attempting to claim job %s\n", job->id.c_str());

    ret = pg_claim_job(queue->conn, job->id.c_str(), queue->node_name.c_str(),
                       timeout);
    if (ret < 0) {
        delete job;
        return -1;
    }

    if (ret == 0) {
        daemon_log(6, "job %s was not claimed\n", job->id.c_str());
        delete job;
        return 0;
    }

    daemon_log(6, "job %s claimed\n", job->id.c_str());

    *job_r = job;
    return 1;
}

/**
 * Copy a string.
 *
 * @return false if the string was not modified.
 */
static bool
copy_string(std::string &dest, const char *src)
{
    assert(src != NULL);

    if (dest.compare(src) == 0)
        return false;

    dest = src;
    return true;
}

void queue_set_filter(Queue *queue, const char *plans_include,
                      const char *plans_exclude,
                      const char *plans_lowprio) {
    bool r1 = copy_string(queue->plans_include, plans_include);
    bool r2 = copy_string(queue->plans_exclude, plans_exclude);
    copy_string(queue->plans_lowprio, plans_lowprio);

    if (r1 || r2) {
        if (queue->running)
            queue->interrupt = true;
        else if (queue->fd >= 0)
            queue_run(queue);
    }
}

static void
queue_run_result(Queue *queue, int num, PGresult *result)
{
    int row, ret;
    Job *job;

    for (row = 0; row < num && !queue->disabled && !queue->interrupt; ++row) {
        ret = get_and_claim_job(queue, result, row, "5 minutes", &job);
        if (ret > 0)
            queue->callback(job, queue->ctx);
        else if (ret < 0)
            break;
    }
}

static void
queue_run2(Queue *queue)
{
    PGresult *result;
    int ret, num;
    bool full = false;
    time_t now;

    assert(!queue->disabled);
    assert(queue->running);
    assert(!queue->fd >= 0);

    if (queue->plans_include.empty() ||
        queue->plans_include.compare("{}") == 0 ||
        queue->plans_exclude.empty())
        return;

    /* check expired jobs from all other nodes except us */

    now = time(NULL);
    if (now >= queue->next_expire_check) {
        queue->next_expire_check = now + 60;

        ret = pg_expire_jobs(queue->conn, queue->node_name.c_str());
        if (ret < 0)
            return;

        if (ret > 0) {
            daemon_log(2, "released %d expired jobs\n", ret);
            pg_notify(queue->conn);
        }
    }

    /* query database */

    queue->interrupt = false;

    daemon_log(7, "requesting new jobs from database; plans_include=%s plans_exclude=%s plans_lowprio=%s\n",
               queue->plans_include.c_str(), queue->plans_exclude.c_str(),
               queue->plans_lowprio.c_str());

    num = pg_select_new_jobs(queue->conn,
                             queue->plans_include.c_str(),
                             queue->plans_exclude.c_str(),
                             queue->plans_lowprio.c_str(),
                             16,
                             &result);
    if (num > 0) {
        queue_run_result(queue, num, result);
        PQclear(result);

        if (num == 16)
            full = true;
    }

    if (!queue->disabled && !queue->interrupt &&
        queue->plans_lowprio.compare("{}") != 0) {
        /* now also select plans which are already running */

        daemon_log(7, "requesting new jobs from database II; plans_lowprio=%s\n",
                   queue->plans_lowprio.c_str());

        num = pg_select_new_jobs(queue->conn,
                                 queue->plans_lowprio.c_str(),
                                 queue->plans_exclude.c_str(),
                                 "{}",
                                 16,
                                 &result);
        if (num > 0) {
            queue_run_result(queue, num, result);
            PQclear(result);

            if (num == 16)
                full = true;
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
queue_run(Queue *queue)
{
    assert(!queue->running);
    assert(!queue->fd >= 0);

    if (queue->disabled)
        return;

    queue->running = true;
    queue_run2(queue);
    queue->running = false;

    queue_check_notify(queue);
}

void queue_disable(Queue *queue) {
    if (queue->disabled)
        return;

    queue->disabled = true;
}

void queue_enable(Queue *queue) {
    assert(!queue->running);

    if (!queue->disabled)
        return;

    queue->disabled = false;

    if (queue->fd >= 0)
        queue_run(queue);
}

int job_set_progress(Job *job, unsigned progress,
                     const char *timeout) {
    int ret;

    daemon_log(5, "job %s progress=%u\n", job->id.c_str(), progress);

    ret = pg_set_job_progress(job->queue->conn, job->id.c_str(), progress,
                              timeout);

    queue_check_all(job->queue);

    return ret;
}

int job_rollback(Job **job_r) {
    Job *job;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    if (!queue_autoreconnect(job->queue))
        return -1;

    daemon_log(6, "rolling back job %s\n", job->id.c_str());

    pg_rollback_job(job->queue->conn, job->id.c_str());

    pg_notify(job->queue->conn);

    queue_check_all(job->queue);

    delete job;

    return 0;
}

int job_done(Job **job_r, int status) {
    Job *job;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    if (!queue_autoreconnect(job->queue))
        return -1;

    daemon_log(6, "job %s done with status %d\n", job->id.c_str(), status);

    pg_set_job_done(job->queue->conn, job->id.c_str(), status);

    queue_check_all(job->queue);

    delete job;

    return 0;
}
