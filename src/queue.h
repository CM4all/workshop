/*
 * Implementation of the job queue with PostgreSQL and async notifies.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef __WORKSHOP_QUEUE_H
#define __WORKSHOP_QUEUE_H

#include "strarray.h"

struct queue;

struct job {
    struct queue *queue;
    char *id, *plan_name, *syslog_server;
    struct strarray args;
};

typedef void (*queue_callback_t)(struct job *job, void *ctx);

/**
 * Schedule a queue run.  It will occur "very soon" (in a few
 * milliseconds).
 */
void
queue_reschedule(struct queue *queue);

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
               struct queue **queue_r);

/**
 * Close the queue database.
 */
void queue_close(struct queue **queue_r);

/**
 * Configure a "plan" filter.
 */
void queue_set_filter(struct queue *queue, const char *plans_include,
                      const char *plans_exclude,
                      const char *plans_lowprio);

/**
 * Disable the queue, e.g. when the node is busy.
 */
void queue_disable(struct queue *queue);

/**
 * Enable the queue after it has been disabled with queue_disable().
 */
void queue_enable(struct queue *queue);

/**
 * Update the "progress" value of the job.
 *
 * @param job the job
 * @param progress a percent value (0 .. 100)
 * @param timeout the timeout for the next feedback (an interval
 * string that is understood by PostgreSQL)
 * @return 1 on success, 0 if the job was not found, -1 on error
 */
int job_set_progress(struct job *job, unsigned progress,
                     const char *timeout);

/**
 * Disassociate from the job, act as if this node had never claimed
 * it.  It will notify the other workshop nodes.
 *
 * @return 0 on success, -1 on error
 */
int job_rollback(struct job **job_r);

/**
 * Mark the job as "done".
 */
int job_done(struct job **job_r, int status);

#endif
