/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "job.hxx"
#include "queue.hxx"

#include <daemon/log.h>

#include <assert.h>

int
Job::SetProgress(unsigned progress, const char *timeout)
{
    daemon_log(5, "job %s progress=%u\n", id.c_str(), progress);

    return queue->SetJobProgress(*this, progress, timeout);
}

int
job_rollback(Job **job_r) {
    Job *job;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    daemon_log(6, "rolling back job %s\n", job->id.c_str());

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

    daemon_log(6, "job %s done with status %d\n", job->id.c_str(), status);

    if (!job->queue->SetJobDone(*job, status))
        return -1;

    delete job;
    return 0;
}
