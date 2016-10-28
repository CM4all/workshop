/*
 * Implementation of the job queue with PostgreSQL and async notifies.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_QUEUE_HXX
#define CRON_QUEUE_HXX

#include "event/DeferEvent.hxx"
#include "event/TimerEvent.hxx"
#include "pg/AsyncConnection.hxx"

#include <inline/compiler.h>

#include <string>

struct CronJob;
class EventLoop;

class CronQueue final : private AsyncPgConnectionHandler {
    typedef std::function<void(CronJob &&job)> Callback;

    const std::string node_name;

    AsyncPgConnection db;

    const Callback callback;

    /**
     * Used to move CheckNotify() calls out of the current stack
     * frame.
     */
    DeferEvent check_notify_event;

    TimerEvent scheduler_timer, claim_timer;

public:
    CronQueue(EventLoop &event_loop, const char *_node_name,
              const char *conninfo, const char *schema,
              Callback _callback);
    ~CronQueue();

    gcc_pure
    const char *GetNodeName() const {
        return node_name.c_str();
    }

    void Connect() {
        db.Connect();
    }

    void Close();

    bool Claim(const CronJob &job);
    void Finish(const CronJob &job);

private:
    /**
     * Checks everything asynchronously: if the connection has failed,
     * schedule a reconnect.  If there are notifies, schedule a queue run.
     *
     * This is an extended version of queue_check_notify(), to be used by
     * public functions that (unlike the internal functions) do not
     * reschedule.
     */
    void CheckNotify() {
        db.CheckNotify();
    }

    void ScheduleCheckNotify() {
        check_notify_event.Schedule();
    }

    void RunScheduler();
    void ScheduleScheduler(bool immediately);

    void RunClaim();
    void ScheduleClaim(bool immediately);

    /**
     * @return false if no pending job was found
     */
    bool CheckPending();

    /* virtual methods from AsyncPgConnectionHandler */
    void OnConnect() override;
    void OnDisconnect() override;
    void OnNotify(const char *name) override;
    void OnError(const char *prefix, const char *error) override;
};

#endif
