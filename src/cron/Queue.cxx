/*
 * Implementation of the job queue with PostgreSQL and async notifies.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Queue.hxx"
#include "CalculateNextRun.hxx"

#include <daemon/log.h>

#include <string.h>
#include <stdlib.h>

CronQueue::CronQueue(EventLoop &event_loop, const char *_node_name,
                     const char *conninfo, const char *schema)
    :node_name(_node_name),
     db(conninfo, schema, *this),
     check_notify_event(event_loop, BIND_THIS_METHOD(CheckNotify)),
     scheduler_timer(event_loop, BIND_THIS_METHOD(RunScheduler))
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
CronQueue::OnConnect()
{
    const auto result = db.Execute("LISTEN cronjobs_scheduled");
    if (!result.IsCommandSuccessful()) {
        daemon_log(1, "LISTEN failed: %s\n", result.GetErrorMessage());
        return;
    }

    ScheduleScheduler(true);
}

void
CronQueue::OnDisconnect()
{
    daemon_log(4, "disconnected from database\n");

    check_notify_event.Cancel();
    scheduler_timer.Cancel();
}

void
CronQueue::OnNotify(const char *name)
{
    if (strcmp(name, "cronjobs_scheduled") == 0)
        ScheduleScheduler(false);
}

void
CronQueue::OnError(const char *prefix, const char *error)
{
    daemon_log(2, "%s: %s\n", prefix, error);
}
