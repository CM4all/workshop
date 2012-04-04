/*
 * Implementation of the job queue with PostgreSQL and async notifies.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef __WORKSHOP_QUEUE_H
#define __WORKSHOP_QUEUE_H

#include <postgresql/libpq-fe.h>
#include <event.h>

#include <string>

struct Job;

typedef void (*queue_callback_t)(Job *job, void *ctx);

struct Queue {
    std::string node_name;
    PGconn *conn;
    int fd;
    bool disabled, running;

    /** if set to 1, the current queue run should be interrupted, to
        be started again */
    bool interrupt;

    /**
     * For detecting notifies from PostgreSQL.
     */
    struct event read_event;

    /**
     * Timer event for which runs the queue or reconnects to
     * PostgreSQL.
     */
    struct event timer_event;

    std::string plans_include, plans_exclude, plans_lowprio;
    time_t next_expire_check;

    queue_callback_t callback;
    void *ctx;

    Queue(const char *_node_name, queue_callback_t _callback, void *_ctx)
        :node_name(_node_name),
         conn(NULL), fd(-1),
         disabled(false), running(false), interrupt(false),
         next_expire_check(0),
         callback(_callback), ctx(_ctx) {}

    Queue(const Queue &other) = delete;

    ~Queue();

    Queue &operator=(const Queue &other) = delete;

    void OnSocket();
    void OnTimer();

    void ScheduleTimer(const struct timeval &tv) {
        evtimer_del(&timer_event);
        evtimer_add(&timer_event, &tv);
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
    void SetFilter(const char *plans_include, const char *plans_exclude,
                   const char *plans_lowprio);

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

/**
 * Open a queue database.  It will listen for notifications.
 *
 * @param node_name the name of this node (host)
 * @param conninfo the PostgreSQL conninfo string (e.g. "dbname=workshop")
 * @param callback a callback that will be invoked when a new job has
 * been claimed
 * @param ctx a pointer that will be passed to the callback
 * @return 0 on success
 */
int queue_open(const char *node_name, const char *conninfo,
               queue_callback_t callback, void *ctx,
               Queue **queue_r);

#endif
