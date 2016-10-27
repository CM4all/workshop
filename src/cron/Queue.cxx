/*
 * Implementation of the job queue with PostgreSQL and async notifies.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Queue.hxx"

#include <daemon/log.h>

CronQueue::CronQueue(EventLoop &event_loop, const char *_node_name,
                     const char *conninfo, const char *schema)
    :node_name(_node_name),
     db(conninfo, schema, *this),
     check_notify_event(event_loop, BIND_THIS_METHOD(CheckNotify))
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
}

void
CronQueue::OnConnect()
{
    const auto result = db.Execute("LISTEN cronjobs_scheduled");
    if (!result.IsCommandSuccessful()) {
        daemon_log(1, "LISTEN failed: %s\n", result.GetErrorMessage());
        return;
    }
}

void
CronQueue::OnDisconnect()
{
    daemon_log(4, "disconnected from database\n");

    check_notify_event.Cancel();
}

void
CronQueue::OnNotify(const char *name)
{
    (void)name; // TODO
}

void
CronQueue::OnError(const char *prefix, const char *error)
{
    daemon_log(2, "%s: %s\n", prefix, error);
}
