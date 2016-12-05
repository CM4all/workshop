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

#include <inline/compiler.h>

#include <functional>
#include <string>
#include <chrono>

struct WorkshopJob;
class EventLoop;

class WorkshopQueue final : private AsyncPgConnectionHandler {
    typedef std::function<void(WorkshopJob &&job)> Callback;

    const std::string node_name;

    AsyncPgConnection db;

    bool disabled = false, running = false;

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
    WorkshopQueue(EventLoop &event_loop,
                  const char *_node_name,
                  const char *conninfo, const char *schema,
                  Callback _callback);
    ~WorkshopQueue();

    gcc_pure
    const char *GetNodeName() const {
        return node_name.c_str();
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

    int SetJobProgress(const WorkshopJob &job, unsigned progress,
                       const char *timeout);

    /**
     * Disassociate from the job, act as if this node had never
     * claimed it.  It will notify the other workshop nodes.
     *
     * @return true on success
     */
    bool RollbackJob(const WorkshopJob &job);

    bool SetJobDone(const WorkshopJob &job, int status);

private:
    void RunResult(const PgResult &result);
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

    /* virtual methods from AsyncPgConnectionHandler */
    void OnConnect() override;
    void OnDisconnect() override;
    void OnNotify(const char *name) override;
    void OnError(const char *prefix, const char *error) override;
};

#endif