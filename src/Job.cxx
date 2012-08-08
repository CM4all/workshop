/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Job.hxx"
#include "Queue.hxx"

#include <assert.h>

int
Job::SetProgress(unsigned progress, const char *timeout)
{
    return queue->SetJobProgress(*this, progress, timeout);
}

int
job_rollback(Job **job_r) {
    Job *job;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    if (!job->queue->RollbackJob(*job))
        return -1;

    delete job;
    return 0;
}

int job_done(Job **job_r, int status) {
    Job *job;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    if (!job->queue->SetJobDone(*job, status))
        return -1;

    delete job;
    return 0;
}
