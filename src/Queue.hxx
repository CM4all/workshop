/*
 * Implementation of the job queue with PostgreSQL and async notifies.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_QUEUE_HXX
#define WORKSHOP_QUEUE_HXX

#include "event/TimerEvent.hxx"
#include "event/Duration.hxx"
#include "pg/AsyncConnection.hxx"

#include <inline/compiler.h>

#include <functional>
#include <string>
#include <chrono>

struct Job;
class EventLoop;

class Queue : private AsyncPgConnectionHandler {
    typedef std::function<void(Job &&job)> Callback;

    std::string node_name;

    AsyncPgConnection db;

    bool disabled = false, running = false;

    /** if set to true, the current queue run should be interrupted,
        to be started again */
    bool interrupt = false;

    /**
     * Timer event which runs the queue.
     */
    TimerEvent timer_event;

    std::string plans_include, plans_exclude, plans_lowprio;
    std::chrono::steady_clock::time_point next_expire_check =
        std::chrono::steady_clock::time_point::min();

    Callback callback;

public:
    Queue(EventLoop &event_loop,
          const char *_node_name, const char *conninfo, const char *schema,
          Callback _callback);

    Queue(const Queue &other) = delete;

    ~Queue();

    Queue &operator=(const Queue &other) = delete;

    gcc_pure
    const char *GetNodeName() const {
        return node_name.c_str();
    }

    void Connect() {
        db.Connect();
    }

    void Close();

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

    int GetNextScheduled(int *span_r);

    /**
     * Configure a "plan" filter.
     */
    void SetFilter(const char *plans_include, std::string &&plans_exclude,
                   std::string &&plans_lowprio);

    void RunResult(const PgResult &result);
    void Run2();
    void Run();

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

    int SetJobProgress(const Job &job, unsigned progress, const char *timeout);

    /**
     * Disassociate from the job, act as if this node had never
     * claimed it.  It will notify the other workshop nodes.
     *
     * @return true on success
     */
    bool RollbackJob(const Job &job);

    bool SetJobDone(const Job &job, int status);

private:
    /* virtual methods from AsyncPgConnectionHandler */
    void OnConnect() override;
    void OnDisconnect() override;
    void OnNotify(const char *name) override;
    void OnError(const char *prefix, const char *error) override;
};

#endif
