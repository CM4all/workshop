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

static void
queue_timer_event_callback(gcc_unused int fd, gcc_unused short event,
                           void *ctx);

Queue::Queue(const char *_node_name, queue_callback_t _callback, void *_ctx)
    :node_name(_node_name),
     conn(NULL), fd(-1),
     disabled(false), running(false), interrupt(false),
     next_expire_check(0),
     callback(_callback), ctx(_ctx) {
    evtimer_set(&timer_event, queue_timer_event_callback, this);
}

Queue::~Queue()
{
    assert(!running);

    if (fd >= 0)
        event_del(&read_event);

    evtimer_del(&timer_event);

    if (conn != NULL)
        PQfinish(conn);
}

void
Queue::OnSocket()
{
    PQconsumeInput(conn);

    if (!AutoReconnect())
        return;

    if (HasNotify())
        Run();
}

/** the poll() callback handler; this function handles notifies sent
    by the PostgreSQL server */
static void
queue_event_callback(gcc_unused int fd, gcc_unused short event,
                     void *ctx)
{
    Queue *queue = (Queue *)ctx;

    assert(fd == queue->fd);
    assert(!queue->running);

    queue->OnSocket();

    assert(!queue->running);
}

void
Queue::OnTimer()
{
    if (!AutoReconnect())
        return;

    Run();
}

static void
queue_timer_event_callback(gcc_unused int fd, gcc_unused short event,
                           void *ctx)
{
    Queue *queue = (Queue *)ctx;

    assert(!queue->running);

    queue->OnTimer();

    assert(!queue->running);
}

int queue_open(const char *node_name, const char *conninfo,
               queue_callback_t callback, void *ctx,
               Queue **queue_r) {
    int ret;

    Queue *queue = new Queue(node_name, callback, ctx);

    /* connect to PostgreSQL */

    queue->conn = PQconnectdb(conninfo);
    if (queue->conn == NULL) {
        delete queue;
        return ENOMEM;
    }

    if (PQstatus(queue->conn) != CONNECTION_OK) {
        daemon_log(2, "connect to PostgreSQL failed: %s\n",
                   PQerrorMessage(queue->conn));

        static constexpr struct timeval tv { 10, 0 };
        queue->ScheduleTimer(tv);

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
bool
Queue::Reconnect()
{
    /* unregister old socket */

    if (fd >= 0) {
        event_del(&read_event);
        fd = -1;
    }

    /* reconnect */

    PQreset(conn);

    if (PQstatus(conn) != CONNECTION_OK) {
        daemon_log(2, "reconnect to PostgreSQL failed: %s\n",
                   PQerrorMessage(conn));

        static constexpr struct timeval tv { 10, 0 };
        ScheduleTimer(tv);

        return false;
    }

    /* listen on notifications */

    if (pg_listen(conn) < 0)
        daemon_log(1, "re-LISTEN failed\n");

    /* register new socket */

    fd = PQsocket(conn);
    event_set(&read_event, fd, EV_READ|EV_PERSIST,
              queue_event_callback, this);
    event_add(&read_event, NULL);

    Reschedule();

    return true;
}

/**
 * Check the status of the database connection, and reconnect when it
 * has gone bad.
 */
bool
Queue::AutoReconnect()
{
    if (PQstatus(conn) == CONNECTION_OK)
        return true;

    if (fd < 0)
        daemon_log(2, "re-trying to reconnect to PostgreSQL\n");
    else
        daemon_log(2, "connection to PostgreSQL lost; trying to reconnect\n");

    return Reconnect();
}

bool
Queue::HasNotify()
{
    PGnotify *notify;
    bool ret = false;

    while ((notify = PQnotifies(conn)) != NULL) {
        daemon_log(6, "async notify '%s' received from backend pid %d\n",
                   notify->relname, notify->be_pid);
        if (strcmp(notify->relname, "new_job") == 0)
            ret = true;
        PQfreemem(notify);
    }

    return ret;
}

int
Queue::GetNextScheduled(int *span_r)
{
    int ret;
    long span;

    if (plans_include.empty()) {
        *span_r = -1;
        return 0;
    }

    ret = pg_next_scheduled_job(conn, plans_include.c_str(),
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

void
Queue::SetFilter(const char *_plans_include, const char *_plans_exclude,
                 const char *_plans_lowprio)
{
    bool r1 = copy_string(plans_include, _plans_include);
    bool r2 = copy_string(plans_exclude, _plans_exclude);
    copy_string(plans_lowprio, _plans_lowprio);

    if (r1 || r2) {
        if (running)
            interrupt = true;
        else if (fd >= 0)
            Run();
    }
}

void
Queue::RunResult(int num, PGresult *result)
{
    int row, ret;
    Job *job;

    for (row = 0; row < num && !disabled && !interrupt; ++row) {
        ret = get_and_claim_job(this, result, row, "5 minutes", &job);
        if (ret > 0)
            callback(job, ctx);
        else if (ret < 0)
            break;
    }
}

void
Queue::Run2()
{
    PGresult *result;
    int ret, num;
    bool full = false;
    time_t now;

    assert(!disabled);
    assert(running);
    assert(!fd >= 0);

    if (plans_include.empty() ||
        plans_include.compare("{}") == 0 ||
        plans_exclude.empty())
        return;

    /* check expired jobs from all other nodes except us */

    now = time(NULL);
    if (now >= next_expire_check) {
        next_expire_check = now + 60;

        ret = pg_expire_jobs(conn, node_name.c_str());
        if (ret < 0)
            return;

        if (ret > 0) {
            daemon_log(2, "released %d expired jobs\n", ret);
            pg_notify(conn);
        }
    }

    /* query database */

    interrupt = false;

    daemon_log(7, "requesting new jobs from database; plans_include=%s plans_exclude=%s plans_lowprio=%s\n",
               plans_include.c_str(), plans_exclude.c_str(),
               plans_lowprio.c_str());

    num = pg_select_new_jobs(conn,
                             plans_include.c_str(),
                             plans_exclude.c_str(),
                             plans_lowprio.c_str(),
                             16,
                             &result);
    if (num > 0) {
        RunResult(num, result);
        PQclear(result);

        if (num == 16)
            full = true;
    }

    if (!disabled && !interrupt &&
        plans_lowprio.compare("{}") != 0) {
        /* now also select plans which are already running */

        daemon_log(7, "requesting new jobs from database II; plans_lowprio=%s\n",
                   plans_lowprio.c_str());

        num = pg_select_new_jobs(conn,
                                 plans_lowprio.c_str(),
                                 plans_exclude.c_str(),
                                 "{}",
                                 16,
                                 &result);
        if (num > 0) {
            RunResult(num, result);
            PQclear(result);

            if (num == 16)
                full = true;
        }
    }

    /* update timeout */

    if (disabled) {
        daemon_log(7, "queue has been disabled\n");
    } else if (interrupt) {
        /* we have been interrupted: run again in 100ms */
        daemon_log(7, "aborting queue run\n");

        Reschedule();
    } else if (full) {
        /* 16 is our row limit, and exactly 16 rows were returned - we
           suspect there may be more.  schedule next queue run in 1
           second */
        Reschedule();
    } else {
        struct timeval tv;

        GetNextScheduled(&ret);
        if (ret >= 0)
            daemon_log(3, "next scheduled job is in %d seconds\n", ret);
        else
            ret = 600;

        tv.tv_sec = ret;
        tv.tv_usec = 0;
        ScheduleTimer(tv);
    }
}

void
Queue::Run()
{
    assert(!running);
    assert(!fd >= 0);

    if (disabled)
        return;

    running = true;
    Run2();
    running = false;

    CheckNotify();
}

void
Queue::Enable()
{
    assert(!running);

    if (!disabled)
        return;

    disabled = false;

    if (fd >= 0)
        Run();
}

int
Queue::SetJobProgress(const Job &job, unsigned progress, const char *timeout)
{
    assert(job.queue == this);

    int ret = pg_set_job_progress(conn, job.id.c_str(), progress, timeout);

    CheckAll();

    return ret;
}

int job_set_progress(Job *job, unsigned progress,
                     const char *timeout) {
    daemon_log(5, "job %s progress=%u\n", job->id.c_str(), progress);

    return job->queue->SetJobProgress(*job, progress, timeout);
}

bool
Queue::RollbackJob(const Job &job)
{
    assert(job.queue == this);

    if (!AutoReconnect())
        return false;

    pg_rollback_job(conn, job.id.c_str());
    pg_notify(conn);

    CheckAll();

    return true;
}

int job_rollback(Job **job_r) {
    Job *job;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    daemon_log(6, "rolling back job %s\n", job->id.c_str());

    if (!job->queue->RollbackJob(*job))
        return -1;

    delete job;
    return 0;
}

bool
Queue::SetJobDone(const Job &job, int status)
{
    assert(job.queue == this);

    if (!AutoReconnect())
        return false;

    pg_set_job_done(conn, job.id.c_str(), status);

    CheckAll();
    return true;
}

int job_done(Job **job_r, int status) {
    Job *job;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    daemon_log(6, "job %s done with status %d\n", job->id.c_str(), status);

    if (!job->queue->SetJobDone(*job, status))
        return -1;

    delete job;
    return 0;
}
