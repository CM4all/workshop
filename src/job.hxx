/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_JOB_HXX
#define WORKSHOP_JOB_HXX

#include "strarray.h"

struct queue;

struct job {
    struct queue *queue;
    char *id, *plan_name, *syslog_server;
    struct strarray args;
};

void
free_job(struct job **job_r);

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
