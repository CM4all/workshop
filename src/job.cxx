/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "job.hxx"

#include <glib.h>

#include <assert.h>

void
free_job(struct job **job_r)
{
    struct job *job;

    assert(job_r != NULL);
    assert(*job_r != NULL);

    job = *job_r;
    *job_r = NULL;

    g_free(job->id);
    g_free(job->plan_name);
    g_free(job->syslog_server);

    strarray_free(&job->args);

    g_free(job);
}
