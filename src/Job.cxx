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

bool
Job::SetDone(int status)
{
    return queue->SetJobDone(*this, status);
}
