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

void
queue_reschedule(struct queue *queue);

int queue_open(const char *node_name, const char *conninfo,
               queue_callback_t callback, void *ctx,
               struct queue **queue_r);

void queue_close(struct queue **queue_r);

void queue_set_filter(struct queue *queue, const char *plans_include,
                      const char *plans_exclude,
                      const char *plans_lowprio);

void queue_disable(struct queue *queue);

void queue_enable(struct queue *queue);

int job_set_progress(struct job *job, unsigned progress,
                     const char *timeout);

int job_rollback(struct job **job_r);

int job_done(struct job **job_r, int status);

#endif
