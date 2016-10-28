/*
 * Implementation of the job queue with PostgreSQL and async notifies.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Queue.hxx"
#include "Job.hxx"
#include "CalculateNextRun.hxx"

#include <daemon/log.h>

#include <string.h>
#include <stdlib.h>

CronQueue::CronQueue(EventLoop &event_loop, const char *_node_name,
                     const char *conninfo, const char *schema,
                     Callback _callback)
    :node_name(_node_name),
     db(conninfo, schema, *this),
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
CronQueue::RunScheduler()
{
    daemon_log(4, "scheduler\n");

    if (!CalculateNextRun(db))
        ScheduleScheduler(false);
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

void
CronQueue::RunClaim()
{
    daemon_log(4, "claim\n");

    if (CheckPending())
        ScheduleClaim(false);
}

void
CronQueue::ScheduleClaim(bool immediately)
{
    struct timeval tv;
    if (immediately) {
        tv.tv_sec = 0;
        tv.tv_usec = 0;
    } else {
        /* randomize the claim to reduce race conditions with
           other nodes */
        const long r = random();
        tv.tv_sec = (r / 1000000) % 30;
        tv.tv_usec = r % 1000000;
    }

    claim_timer.Add(tv);
}

bool
CronQueue::Claim(const CronJob &job)
{
    const char *timeout = "5 minutes";

    const auto r =
        db.ExecuteParams("UPDATE cronjobs "
                         "SET node_name=$2, node_timeout=NOW()+$3::INTERVAL "
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
    const auto r =
        db.ExecuteParams("UPDATE cronjobs "
                         "SET node_name=NULL, node_timeout=NULL "
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

bool
CronQueue::CheckPending()
{
    const auto result =
        db.Execute("SELECT id, account_id, command, translate_param "
                   "FROM cronjobs WHERE enabled AND next_run<=NOW() "
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
        job.account_id = row.GetValue(0);
        job.command = row.GetValue(0);
        job.translate_param = row.GetValue(0);

        callback(std::move(job));
    }

    return true;
}

void
CronQueue::OnConnect()
{
    const auto result = db.Execute("LISTEN cronjobs_scheduled");
    if (!result.IsCommandSuccessful()) {
        daemon_log(1, "LISTEN failed: %s\n", result.GetErrorMessage());
        return;
    }

    ScheduleScheduler(true);
    ScheduleClaim(true);
}

void
CronQueue::OnDisconnect()
{
    daemon_log(4, "disconnected from database\n");

    check_notify_event.Cancel();
    scheduler_timer.Cancel();
    claim_timer.Cancel();
}

void
CronQueue::OnNotify(const char *name)
{
    if (strcmp(name, "cronjobs_scheduled") == 0) {
        ScheduleScheduler(false);
        CheckPending();
    }
}

void
CronQueue::OnError(const char *prefix, const char *error)
{
    daemon_log(2, "%s: %s\n", prefix, error);
}
