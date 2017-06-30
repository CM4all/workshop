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

#include "util/Compiler.h"

#include <string>

struct CronJob;
class EventLoop;

class CronQueue final : private Pg::AsyncConnectionHandler {
    typedef std::function<void(CronJob &&job)> Callback;

    const std::string node_name;

    Pg::AsyncConnection db;

    const Callback callback;

    /**
     * Used to move CheckNotify() calls out of the current stack
     * frame.
     */
    DeferEvent check_notify_event;

    TimerEvent scheduler_timer, claim_timer;

    bool disabled = false;

public:
    CronQueue(EventLoop &event_loop, const char *_node_name,
              const char *conninfo, const char *schema,
              Callback _callback);
    ~CronQueue();

    EventLoop &GetEventLoop() {
        return check_notify_event.GetEventLoop();
    }

    gcc_pure
    const char *GetNodeName() const {
        return node_name.c_str();
    }

    void Connect() {
        db.Connect();
    }

    void Close();

    gcc_pure
    std::string GetNow() {
        ScheduleCheckNotify();
        const auto result = db.Execute("SELECT now()");
        return result.GetOnlyStringChecked();
    }

    /**
     * Disable the queue, e.g. when the node is busy.
     */
    void Disable() {
        disabled = true;
    }

    /**
     * Enable the queue after it has been disabled with Disable().
     */
    void Enable();

    bool Claim(const CronJob &job);
    void Finish(const CronJob &job);

    /**
     * Insert a row into the "cronresults" table, describing the
     * #CronJob execution result.
     */
    void InsertResult(const CronJob &job, const char *start_time,
                      int exit_status,
                      const char *log);

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

    void ReleaseStale();

    void RunScheduler();
    void ScheduleScheduler(bool immediately);

    void RunClaim();
    void ScheduleClaim();

    /**
     * @return false if no pending job was found
     */
    bool CheckPending();

    /* virtual methods from Pg::AsyncConnectionHandler */
    void OnConnect() override;
    void OnDisconnect() override;
    void OnNotify(const char *name) override;
    void OnError(const char *prefix, const char *error) override;
};

#endif
