/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "job.hxx"

#include <assert.h>

void
free_job(Job **job_r)
{
    Job *job;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    strarray_free(&job->args);
    delete job;
}
