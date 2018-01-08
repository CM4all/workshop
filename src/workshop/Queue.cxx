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
#include "util/RuntimeError.hxx"

#include <stdexcept>

#include <sys/types.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

WorkshopQueue::WorkshopQueue(const Logger &parent_logger,
                             EventLoop &event_loop,
                             const char *_node_name,
                             const char *conninfo, const char *schema,
                             Callback _callback)
    :logger(parent_logger, "queue"), node_name(_node_name),
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

    ret = pg_next_scheduled_job(db,
                                plans_include.c_str(),
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
get_job(WorkshopJob &job,
        const Pg::Result &result, unsigned row)
{
    assert(row < result.GetRowCount());

    job.id = result.GetValue(row, 0);
    job.plan_name = result.GetValue(row, 1);

    job.args = Pg::DecodeArray(result.GetValue(row, 2));
    job.env = Pg::DecodeArray(result.GetValue(row, 4));

    if (!result.IsValueNull(row, 3))
        job.syslog_server = result.GetValue(row, 3);

    return !job.id.empty() && !job.plan_name.empty();
}

static int
get_and_claim_job(const ChildLogger &logger, WorkshopJob &job,
                  const char *node_name,
                  Pg::Connection &db,
                  const Pg::Result &result, unsigned row,
                  const char *timeout) {
    try {
        if (!get_job(job, result, row))
            return -1;
    } catch (...) {
        logger(1, "Failed to load job from database record",
               std::current_exception());
        return -1;
    }

    int ret;

    logger(6, "attempting to claim job ", job.id);

    ret = pg_claim_job(db, job.id.c_str(), node_name, timeout);
    if (ret < 0)
        return -1;

    if (ret == 0) {
        logger(6, "job ", job.id, " was not claimed");
        return 0;
    }

    logger(6, "job ", job.id, " claimed");
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
    if (dest == src)
        return false;

    dest = std::move(src);
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
         row != end && !IsDisabled() && !interrupt; ++row) {
        WorkshopJob job(*this);
        int ret = get_and_claim_job(logger, job,
                                    GetNodeName(),
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

    assert(!IsDisabled());
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
            logger(2, "released ", ret, " expired jobs");
            pg_notify(db);
        }
    }

    /* query database */

    interrupt = false;

    logger(7, "requesting new jobs from database; "
           "plans_include=", plans_include,
           " plans_exclude=", plans_exclude,
           " plans_lowprio=", plans_lowprio);

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

    if (!IsDisabled() && !interrupt &&
        plans_lowprio.compare("{}") != 0) {
        /* now also select plans which are already running */

        logger(7, "requesting new jobs from database II; plans_lowprio=",
               plans_lowprio);

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

    if (IsDisabled()) {
        logger(7, "queue has been disabled");
    } else if (interrupt) {
        /* we have been interrupted: run again in 100ms */
        logger(7, "aborting queue run");

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
            logger(3, "next scheduled job is in ", ret, " seconds");
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

    if (IsDisabled())
        return;

    ScheduleCheckNotify();

    running = true;
    Run2();
    running = false;
}

void
WorkshopQueue::EnableAdmin()
{
    assert(!running);

    if (!disabled_admin)
        return;

    disabled_admin = false;

    if (!IsDisabled() && db.IsReady())
        Reschedule();
}

void
WorkshopQueue::EnableFull()
{
    assert(!running);

    if (!disabled_full)
        return;

    disabled_full = false;

    if (!IsDisabled() && db.IsReady())
        Reschedule();
}

int
WorkshopQueue::SetJobProgress(const WorkshopJob &job, unsigned progress,
                              const char *timeout)
{
    assert(&job.queue == this);

    logger(5, "job ", job.id, " progress=", progress);

    ScheduleCheckNotify();

    return pg_set_job_progress(db, job.id.c_str(), progress, timeout);
}

void
WorkshopQueue::SetJobEnv(const WorkshopJob &job, const char *more_env)
{
    assert(&job.queue == this);

    ScheduleCheckNotify();

    return PgSetEnv(db, job.id.c_str(), more_env);
}

void
WorkshopQueue::RollbackJob(const WorkshopJob &job) noexcept
{
    assert(&job.queue == this);

    logger(6, "rolling back job ", job.id);

    try {
        pg_rollback_job(db, job.id.c_str());
    } catch (...) {
        logger(1, "Failed to roll back job '", job.id, "': ",
               std::current_exception());
    }

    pg_notify(db);

    ScheduleCheckNotify();
}

void
WorkshopQueue::AgainJob(const WorkshopJob &job,
                        std::chrono::seconds delay) noexcept
{
    assert(&job.queue == this);

    logger(6, "rescheduling job ", job.id);

    try {
        if (delay > std::chrono::seconds())
            pg_again_job(db, job.id.c_str(), delay);
        else
            pg_rollback_job(db, job.id.c_str());
    } catch (...) {
        logger(1, "Failed to reschedule job '", job.id, "': ",
               std::current_exception());
    }

    pg_notify(db);

    ScheduleCheckNotify();
}

void
WorkshopQueue::SetJobDone(const WorkshopJob &job, int status,
                          const char *log) noexcept
{
    assert(&job.queue == this);

    logger(6, "job ", job.id, " done with status ", status);

    try {
        pg_set_job_done(db, job.id.c_str(), status, log);
    } catch (...) {
        logger(1, "Failed to mark job '", job.id, "' done: ",
               std::current_exception());
    }

    ScheduleCheckNotify();
}

void
WorkshopQueue::OnConnect()
{
    static constexpr const char *const required_jobs_columns[] = {
        "enabled",
        "log",
    };

    const char *schema = db.GetSchemaName().empty()
        ? "public"
        : db.GetSchemaName().c_str();

    for (const char *name : required_jobs_columns)
        if (!Pg::ColumnExists(db, schema, "jobs", name))
            throw FormatRuntimeError("No column 'jobs.%s'; please migrate the database",
                                     name);

    db.ExecuteOrThrow("LISTEN new_job");

    int ret = pg_release_jobs(db, node_name.c_str());
    if (ret > 0) {
        logger(2, "released ", ret, " stale jobs");
        pg_notify(db);
    }

    Reschedule();
}

void
WorkshopQueue::OnDisconnect() noexcept
{
    logger(4, "disconnected from database");

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
WorkshopQueue::OnError(std::exception_ptr e) noexcept
{
    logger(1, e);
}
