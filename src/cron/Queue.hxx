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
#include "io/Logger.hxx"
#include "util/Compiler.h"

#include <string>
#include <functional>

struct CronJob;
class EventLoop;

class CronQueue final : private Pg::AsyncConnectionHandler {
    typedef std::function<void(CronJob &&job)> Callback;

    const std::string node_name;

    ChildLogger logger;

    Pg::AsyncConnection db;

    const Callback callback;

    /**
     * Used to move CheckNotify() calls out of the current stack
     * frame.
     */
    DeferEvent check_notify_event;

    TimerEvent scheduler_timer, claim_timer;

    /**
     * Was the queue disabled by the administrator?  Also used during
     * shutdown.
     */
    bool disabled_admin = false;

    /**
     * Is the queue disabled because the node is busy and all slots
     * are full?
     */
    bool disabled_full = false;

public:
    CronQueue(const Logger &parent_logger,
              EventLoop &event_loop, const char *_node_name,
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

    bool IsDisabled() const {
        return disabled_admin || disabled_full;
    }

    /**
     * Disable the queue as an administrative decision (e.g. daemon
     * shutdown).
     */
    void DisableAdmin() {
        disabled_admin = true;
    }

    /**
     * Enable the queue after it has been disabled with DisableAdmin().
     */
    void EnableAdmin();

    /**
     * Disable the queue, e.g. when the node is busy.
     */
    void DisableFull() {
        disabled_full = true;
    }

    /**
     * Enable the queue after it has been disabled with DisableFull().
     */
    void EnableFull();

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
    void OnDisconnect() noexcept override;
    void OnNotify(const char *name) override;
    void OnError(std::exception_ptr e) noexcept override;
};

#endif
