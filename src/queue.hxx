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

    ~Queue();
};

/**
 * Schedule a queue run.  It will occur "very soon" (in a few
 * milliseconds).
 */
void
queue_reschedule(Queue *queue);

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

/**
 * Close the queue database.
 */
void queue_close(Queue **queue_r);

/**
 * Configure a "plan" filter.
 */
void queue_set_filter(Queue *queue, const char *plans_include,
                      const char *plans_exclude,
                      const char *plans_lowprio);

/**
 * Disable the queue, e.g. when the node is busy.
 */
void queue_disable(Queue *queue);

/**
 * Enable the queue after it has been disabled with queue_disable().
 */
void queue_enable(Queue *queue);

#endif
