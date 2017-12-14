/*
 * Implementation of the job queue with PostgreSQL and async notifies.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Queue.hxx"
#include "Job.hxx"
#include "CalculateNextRun.hxx"
#include "event/Duration.hxx"

#include <chrono>

#include <string.h>
#include <stdlib.h>

CronQueue::CronQueue(const Logger &parent_logger,
                     EventLoop &event_loop, const char *_node_name,
                     const char *conninfo, const char *schema,
                     Callback _callback)
    :node_name(_node_name),
     logger(parent_logger, "queue"),
     db(event_loop, conninfo, schema, *this),
     callback(_callback),
     check_notify_event(event_loop, BIND_THIS_METHOD(CheckNotify)),
     scheduler_timer(event_loop, BIND_THIS_METHOD(RunScheduler)),
     claim_timer(event_loop, BIND_THIS_METHOD(RunClaim))
{
}

CronQueue::~CronQueue()
{
    Close();
}

void
CronQueue::Close()
{
    db.Disconnect();

    check_notify_event.Cancel();
    scheduler_timer.Cancel();
    claim_timer.Cancel();
}

void
CronQueue::EnableAdmin()
{
    if (!disabled_admin)
        return;

    disabled_admin = false;
    if (IsDisabled() || !db.IsReady())
        return;

    ScheduleClaim();
}

void
CronQueue::EnableFull()
{
    if (!disabled_full)
        return;

    disabled_full = false;
    if (IsDisabled() || !db.IsReady())
        return;

    ScheduleClaim();
}

void
CronQueue::ReleaseStale()
{
    const auto result =
        db.ExecuteParams("UPDATE cronjobs "
                         "SET node_name=NULL, node_timeout=NULL, next_run=NULL "
                         "WHERE node_name=$1",
                         node_name.c_str());
    if (!result.IsCommandSuccessful()) {
        fprintf(stderr, "UPDATE/stale on cronjobs failed: %s\n",
                result.GetErrorMessage());
        return;
    }

    unsigned n = result.GetAffectedRows();
    if (n > 0)
        logger(3, "Released ", n, " stale cronjobs");
}

void
CronQueue::RunScheduler()
{
    logger(4, "scheduler");

    if (!CalculateNextRun(logger, db))
        ScheduleScheduler(false);

    ScheduleCheckNotify();
}

void
CronQueue::ScheduleScheduler(bool immediately)
{
    struct timeval tv;
    if (immediately) {
        tv.tv_sec = 0;
        tv.tv_usec = 0;
    } else {
        /* randomize the scheduler to reduce race conditions with
           other nodes */
        const long r = random();
        tv.tv_sec = (r / 1000000) % 5;
        tv.tv_usec = r % 1000000;
    }

    scheduler_timer.Add(tv);
}

static std::chrono::seconds
FindEarliestPending(Pg::Connection &db)
{
    const auto result =
        db.Execute("SELECT EXTRACT(EPOCH FROM (MIN(next_run) - now())) FROM cronjobs "
                   "WHERE enabled AND next_run IS NOT NULL AND next_run != 'infinity' AND node_name IS NULL");
    if (!result.IsQuerySuccessful()) {
        fprintf(stderr, "SELECT FROM cronjobs failed: %s\n",
                result.GetErrorMessage());
        return std::chrono::minutes(1);
    }

    if (result.IsEmpty() || result.IsValueNull(0, 0))
        /* no matching cronjob; disable the timer, and wait for the
           next PostgreSQL notify */
        return std::chrono::seconds::max();

    const char *s = result.GetValue(0, 0);
    char *endptr;
    long long value = strtoull(s, &endptr, 10);
    if (endptr == s)
        return std::chrono::minutes(1);

    return std::min<std::chrono::seconds>(std::chrono::seconds(value),
                                          std::chrono::hours(24));
}

void
CronQueue::RunClaim()
{
    if (IsDisabled())
        return;

    logger(4, "claim");

    auto delta = FindEarliestPending(db);
    if (delta == delta.max())
        return;

    if (delta > delta.zero()) {
        /* randomize the claim to reduce race conditions with
           other nodes */
        const long r = random();
        struct timeval tv;
        tv.tv_sec = delta.count() + (r / 1000000) % 30;
        tv.tv_usec = r % 1000000;
        claim_timer.Add(tv);
        return;
    }

    CheckPending();

    ScheduleClaim();
    ScheduleCheckNotify();
}

void
CronQueue::ScheduleClaim()
{
    claim_timer.Add(EventDuration<1>::value);
}

bool
CronQueue::Claim(const CronJob &job)
{
    const char *timeout = "5 minutes";

    const auto r =
        db.ExecuteParams("UPDATE cronjobs "
                         "SET node_name=$2, node_timeout=now()+$3::INTERVAL "
                         "WHERE id=$1 AND enabled AND node_name IS NULL",
                         job.id.c_str(),
                         node_name.c_str(),
                         timeout);
    if (!r.IsCommandSuccessful()) {
        fprintf(stderr, "UPDATE/claim on cronjobs failed: %s\n",
                r.GetErrorMessage());
        return false;
    }

    if (r.GetAffectedRows() == 0) {
        fprintf(stderr, "Lost race to run job '%s'\n", job.id.c_str());
        return false;
    }

    return true;
}

void
CronQueue::Finish(const CronJob &job)
{
    ScheduleCheckNotify();

    const auto r =
        db.ExecuteParams("UPDATE cronjobs "
                         "SET node_name=NULL, node_timeout=NULL, last_run=now(), next_run=NULL "
                         "WHERE id=$1 AND node_name=$2",
                         job.id.c_str(),
                         node_name.c_str());
    if (!r.IsCommandSuccessful()) {
        fprintf(stderr, "UPDATE/finish on cronjobs failed: %s\n",
                r.GetErrorMessage());
        return;
    }

    if (r.GetAffectedRows() == 0) {
        fprintf(stderr, "Lost race to finish job '%s'\n", job.id.c_str());
        return;
    }
}

void
CronQueue::InsertResult(const CronJob &job, const char *start_time,
                        int exit_status, const char *log)
{
    ScheduleCheckNotify();

    const auto r =
        db.ExecuteParams("INSERT INTO cronresults(cronjob_id, node_name, start_time, exit_status, log) "
                         "VALUES($1, $2, $3, $4, $5)",
                         job.id.c_str(),
                         node_name.c_str(),
                         start_time,
                         exit_status,
                         log);
    if (!r.IsCommandSuccessful()) {
        fprintf(stderr, "INSERT on cronresults failed: %s\n",
                r.GetErrorMessage());
        return;
    }
}

bool
CronQueue::CheckPending()
{
    if (IsDisabled())
        return false;

    const auto result =
        db.Execute("SELECT id, account_id, command, translate_param, notification "
                   "FROM cronjobs WHERE enabled AND next_run<=now() "
                   "AND node_name IS NULL "
                   "LIMIT 1");
    if (!result.IsQuerySuccessful()) {
        fprintf(stderr, "SELECT on cronjobs failed: %s\n",
                result.GetErrorMessage());
        return false;
    }

    if (result.IsEmpty())
        return false;

    for (const auto &row : result) {
        CronJob job;
        job.id = row.GetValue(0);
        job.account_id = row.GetValue(1);
        job.command = row.GetValue(2);
        job.translate_param = row.GetValue(3);
        job.notification = row.GetValue(4);

        callback(std::move(job));

        if (IsDisabled())
            return false;
    }

    return true;
}

void
CronQueue::OnConnect()
{
    auto result = db.Execute("LISTEN cronjobs_modified");
    if (!result.IsCommandSuccessful()) {
        logger(1, "LISTEN failed: ", result.GetErrorMessage());
        return;
    }

    result = db.Execute("LISTEN cronjobs_scheduled");
    if (!result.IsCommandSuccessful()) {
        logger(1, "LISTEN failed: ", result.GetErrorMessage());
        return;
    }

    /* internally, all time stamps should be UTC, and PostgreSQL
       should not mangle those time stamps to the time zone that our
       process happens to be configured for */
    result = db.Execute("SET timezone='UTC'");
    if (!result.IsCommandSuccessful())
        logger(1, "SET timezone failed: ", result.GetErrorMessage());

    ReleaseStale();

    ScheduleScheduler(true);
    ScheduleClaim();
    ScheduleCheckNotify();
}

void
CronQueue::OnDisconnect()
{
    logger(4, "disconnected from database");

    check_notify_event.Cancel();
    scheduler_timer.Cancel();
    claim_timer.Cancel();
}

void
CronQueue::OnNotify(const char *name)
{
    if (strcmp(name, "cronjobs_modified") == 0)
        ScheduleScheduler(false);
    else if (strcmp(name, "cronjobs_scheduled") == 0)
        ScheduleClaim();
}

void
CronQueue::OnError(std::exception_ptr e)
{
    logger(1, e);
}
