/*
 * Implementation of the job queue with PostgreSQL and async notifies.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Queue.hxx"
#include "PGQueue.hxx"
#include "Job.hxx"
#include "pg/Array.hxx"
#include "pg/Reflection.hxx"

#include "util/Compiler.h"
#include <daemon/log.h>

#include <stdexcept>

#include <stdbool.h>
#include <sys/types.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

WorkshopQueue::WorkshopQueue(EventLoop &event_loop,
                             const char *_node_name,
                             const char *conninfo, const char *schema,
                             Callback _callback)
    :node_name(_node_name),
     db(event_loop, conninfo, schema, *this),
     check_notify_event(event_loop, BIND_THIS_METHOD(CheckNotify)),
     timer_event(event_loop, BIND_THIS_METHOD(OnTimer)),
     callback(_callback) {
}

WorkshopQueue::~WorkshopQueue()
{
    assert(!running);

    Close();
}

void
WorkshopQueue::Close()
{
    assert(!running);

    db.Disconnect();

    timer_event.Cancel();
    check_notify_event.Cancel();
}

void
WorkshopQueue::OnTimer()
{
    Run();
}

int
WorkshopQueue::GetNextScheduled(int *span_r)
{
    int ret;
    long span;

    if (plans_include.empty()) {
        *span_r = -1;
        return 0;
    }

    ret = pg_next_scheduled_job(db, plans_include.c_str(),
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
get_job(WorkshopJob &job, const Pg::Result &result, unsigned row)
{
    assert(row < result.GetRowCount());

    job.id = result.GetValue(row, 0);
    job.plan_name = result.GetValue(row, 1);

    std::list<std::string> args;
    try {
        args = Pg::DecodeArray(result.GetValue(row, 2));
    } catch (const std::invalid_argument &e) {
        daemon_log(1, "pg_decode_array() failed: %s\n", e.what());
        return false;
    }

    job.args.splice(job.args.end(), args, args.begin(), args.end());

    if (!result.IsValueNull(row, 3))
        job.syslog_server = result.GetValue(row, 3);

    return !job.id.empty() && !job.plan_name.empty();
}

static int
get_and_claim_job(WorkshopJob &job, const char *node_name,
                  Pg::Connection &db,
                  const Pg::Result &result, unsigned row,
                  const char *timeout) {
    if (!get_job(job, result, row))
        return -1;

    int ret;

    daemon_log(6, "attempting to claim job %s\n", job.id.c_str());

    ret = pg_claim_job(db, job.id.c_str(), node_name, timeout);
    if (ret < 0)
        return -1;

    if (ret == 0) {
        daemon_log(6, "job %s was not claimed\n", job.id.c_str());
        return 0;
    }

    daemon_log(6, "job %s claimed\n", job.id.c_str());
    return 1;
}

/**
 * Copy a string.
 *
 * @return false if the string was not modified.
 */
static bool
copy_string(std::string &dest, std::string &&src)
{
    if (dest.compare(src) == 0)
        return false;

    dest = src;
    return true;
}

void
WorkshopQueue::SetFilter(std::string &&_plans_include,
                         std::string &&_plans_exclude,
                         std::string &&_plans_lowprio)
{
    bool r1 = copy_string(plans_include, std::move(_plans_include));
    bool r2 = copy_string(plans_exclude, std::move(_plans_exclude));
    plans_lowprio = std::move(_plans_lowprio);

    if (r1 || r2) {
        if (running)
            interrupt = true;
        else if (db.IsReady())
            Reschedule();
    }
}

void
WorkshopQueue::RunResult(const Pg::Result &result)
{
    for (unsigned row = 0, end = result.GetRowCount();
         row != end && !disabled && !interrupt; ++row) {
        WorkshopJob job(*this);
        int ret = get_and_claim_job(job, GetNodeName(),
                                    db, result, row, "5 minutes");
        if (ret > 0)
            callback(std::move(job));
        else if (ret < 0)
            break;
    }
}

void
WorkshopQueue::Run2()
{
    int ret;
    bool full = false;

    assert(!disabled);
    assert(running);

    if (plans_include.empty() ||
        plans_include.compare("{}") == 0 ||
        plans_exclude.empty())
        return;

    /* check expired jobs from all other nodes except us */

    const auto now = std::chrono::steady_clock::now();
    if (now >= next_expire_check) {
        next_expire_check = now + std::chrono::seconds(60);

        ret = pg_expire_jobs(db, node_name.c_str());
        if (ret < 0)
            return;

        if (ret > 0) {
            daemon_log(2, "released %d expired jobs\n", ret);
            pg_notify(db);
        }
    }

    /* query database */

    interrupt = false;

    daemon_log(7, "requesting new jobs from database; plans_include=%s plans_exclude=%s plans_lowprio=%s\n",
               plans_include.c_str(), plans_exclude.c_str(),
               plans_lowprio.c_str());

    constexpr unsigned MAX_JOBS = 16;
    auto result =
        pg_select_new_jobs(db,
                           plans_include.c_str(), plans_exclude.c_str(),
                           plans_lowprio.c_str(),
                           MAX_JOBS);
    if (result.IsQuerySuccessful() && !result.IsEmpty()) {
        RunResult(result);

        if (result.GetRowCount() == MAX_JOBS)
            full = true;
    }

    if (!disabled && !interrupt &&
        plans_lowprio.compare("{}") != 0) {
        /* now also select plans which are already running */

        daemon_log(7, "requesting new jobs from database II; plans_lowprio=%s\n",
                   plans_lowprio.c_str());

        result = pg_select_new_jobs(db,
                                    plans_lowprio.c_str(),
                                    plans_exclude.c_str(),
                                    "{}",
                                    MAX_JOBS);
        if (result.IsQuerySuccessful() && !result.IsEmpty()) {
            RunResult(result);

            if (result.GetRowCount() == MAX_JOBS)
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
WorkshopQueue::Run()
{
    assert(!running);

    if (disabled)
        return;

    ScheduleCheckNotify();

    running = true;
    Run2();
    running = false;
}

void
WorkshopQueue::Enable()
{
    assert(!running);

    if (!disabled)
        return;

    disabled = false;

    if (db.IsReady())
        Reschedule();
}

int
WorkshopQueue::SetJobProgress(const WorkshopJob &job, unsigned progress,
                              const char *timeout)
{
    assert(&job.queue == this);

    daemon_log(5, "job %s progress=%u\n", job.id.c_str(), progress);

    ScheduleCheckNotify();

    return pg_set_job_progress(db, job.id.c_str(), progress, timeout);
}

bool
WorkshopQueue::RollbackJob(const WorkshopJob &job)
{
    assert(&job.queue == this);

    daemon_log(6, "rolling back job %s\n", job.id.c_str());

    pg_rollback_job(db, job.id.c_str());
    pg_notify(db);

    ScheduleCheckNotify();

    return true;
}

bool
WorkshopQueue::SetJobDone(const WorkshopJob &job, int status, const char *log)
{
    assert(&job.queue == this);

    daemon_log(6, "job %s done with status %d\n", job.id.c_str(), status);

    pg_set_job_done(db, job.id.c_str(), status, log);

    ScheduleCheckNotify();
    return true;
}

void
WorkshopQueue::OnConnect()
{
    int ret = pg_release_jobs(db, node_name.c_str());
    if (ret > 0) {
        daemon_log(2, "released %d stale jobs\n", ret);
        pg_notify(db);
    }

    has_log_column = Pg::ColumnExists(db, "public", "jobs", "log");

    /* listen on notifications */

    if (!pg_listen(db))
        throw std::runtime_error("LISTEN failed");

    Reschedule();
}

void
WorkshopQueue::OnDisconnect()
{
    daemon_log(4, "disconnected from database\n");

    timer_event.Cancel();
    check_notify_event.Cancel();
}

void
WorkshopQueue::OnNotify(const char *name)
{
    if (strcmp(name, "new_job") == 0)
        Reschedule();
}

void
WorkshopQueue::OnError(const char *prefix, const char *error)
{
    daemon_log(2, "%s: %s\n", prefix, error);
}
