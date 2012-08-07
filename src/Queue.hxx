/*
 * Implementation of the job queue with PostgreSQL and async notifies.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_QUEUE_HXX
#define WORKSHOP_QUEUE_HXX

#include "Event.hxx"

#include <postgresql/libpq-fe.h>

#include <functional>
#include <string>

struct Job;

struct Queue {
    typedef std::function<void(Job *job)> Callback;

    std::string node_name;
    PGconn *conn;
    int fd = -1;
    bool disabled = false, running = false;

    /** if set to true, the current queue run should be interrupted,
        to be started again */
    bool interrupt = false;

    /**
     * For detecting notifies from PostgreSQL.
     */
    Event read_event;

    /**
     * Timer event for which runs the queue or reconnects to
     * PostgreSQL.
     */
    Event timer_event;

    std::string plans_include, plans_exclude, plans_lowprio;
    time_t next_expire_check = 0;

    Callback callback;

    Queue(const char *_node_name, const char *conninfo, Callback _callback);

    Queue(const Queue &other) = delete;

    ~Queue();

    Queue &operator=(const Queue &other) = delete;

    void Close();

    void OnSocket();
    void OnTimer();

    void ScheduleTimer(const struct timeval &tv) {
        timer_event.Delete();
        timer_event.Add(&tv);
    }

    /**
     * Schedule a queue run.  It will occur "very soon" (in a few
     * milliseconds).
     */
    void Reschedule() {
        static constexpr struct timeval tv { 0, 10000 };
        ScheduleTimer(tv);
    }

    bool Reconnect();
    bool AutoReconnect();

    bool HasNotify();

    void CheckNotify() {
        if (HasNotify())
            /* there are pending notifies - set a very short timeout,
               so libevent will call us very soon */
            Reschedule();
    }

    /**
     * Checks everything asynchronously: if the connection has failed,
     * schedule a reconnect.  If there are notifies, schedule a queue run.
     *
     * This is an extended version of queue_check_notify(), to be used by
     * public functions that (unlike the internal functions) do not
     * reschedule.
     */
    void CheckAll() {
        if (HasNotify() || PQstatus(conn) != CONNECTION_OK)
            /* something needs to be done - schedule it for the timer
               event callback */
            Reschedule();
    }

    int GetNextScheduled(int *span_r);

    /**
     * Configure a "plan" filter.
     */
    void SetFilter(const char *plans_include, std::string &&plans_exclude,
                   std::string &&plans_lowprio);

    void RunResult(int num, PGresult *result);
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
    bool RollbackJob(const Job &job);
    bool SetJobDone(const Job &job, int status);
};

#endif
