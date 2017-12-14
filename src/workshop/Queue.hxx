/*
 * Implementation of the job queue with PostgreSQL and async notifies.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_QUEUE_HXX
#define WORKSHOP_QUEUE_HXX

#include "event/DeferEvent.hxx"
#include "event/TimerEvent.hxx"
#include "event/Duration.hxx"
#include "pg/AsyncConnection.hxx"
#include "io/Logger.hxx"
#include "util/Compiler.h"

#include <functional>
#include <string>
#include <chrono>

struct WorkshopJob;
class EventLoop;

class WorkshopQueue final : private Pg::AsyncConnectionHandler {
    typedef std::function<void(WorkshopJob &&job)> Callback;

    const ChildLogger logger;

    const std::string node_name;

    Pg::AsyncConnection db;

    bool has_enabled_column, has_log_column;

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

    bool running = false;

    /** if set to true, the current queue run should be interrupted,
        to be started again */
    bool interrupt = false;

    /**
     * Used to move CheckNotify() calls out of the current stack
     * frame.
     */
    DeferEvent check_notify_event;

    /**
     * Timer event which runs the queue.
     */
    TimerEvent timer_event;

    std::string plans_include, plans_exclude, plans_lowprio;
    std::chrono::steady_clock::time_point next_expire_check =
        std::chrono::steady_clock::time_point::min();

    const Callback callback;

public:
    WorkshopQueue(const Logger &parent_logger, EventLoop &event_loop,
                  const char *_node_name,
                  const char *conninfo, const char *schema,
                  Callback _callback);
    ~WorkshopQueue();

    gcc_pure
    const char *GetNodeName() const {
        return node_name.c_str();
    }

    bool HasEnabledColumn() const {
        return has_enabled_column;
    }

    bool HasLogColumn() const {
        return has_log_column;
    }

    void Connect() {
        db.Connect();
    }

    void Close();

    /**
     * Configure a "plan" filter.
     */
    void SetFilter(std::string &&plans_include, std::string &&plans_exclude,
                   std::string &&plans_lowprio);

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

    int SetJobProgress(const WorkshopJob &job, unsigned progress,
                       const char *timeout);

    /**
     * Disassociate from the job, act as if this node had never
     * claimed it.  It will notify the other workshop nodes.
     */
    void RollbackJob(const WorkshopJob &job) noexcept;

    /**
     * Reschedule the given job after it has been executed already.
     *
     * @param delay don't execute this job until the given duration
     * has passed
     */
    void AgainJob(const WorkshopJob &job, std::chrono::seconds delay) noexcept;

    void SetJobDone(const WorkshopJob &job, int status, const char *log);

private:
    void RunResult(const Pg::Result &result);
    void Run2();
    void Run();

    void OnTimer();

    void ScheduleTimer(const struct timeval &tv) {
        timer_event.Add(tv);
    }

    /**
     * Schedule a queue run.  It will occur "very soon" (in a few
     * milliseconds).
     */
    void Reschedule() {
        ScheduleTimer(EventDuration<0, 10000>::value);
    }

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

    int GetNextScheduled(int *span_r);

    /* virtual methods from Pg::AsyncConnectionHandler */
    void OnConnect() override;
    void OnDisconnect() noexcept override;
    void OnNotify(const char *name) override;
    void OnError(std::exception_ptr e) noexcept override;
};

#endif
