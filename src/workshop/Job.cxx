/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Job.hxx"
#include "Queue.hxx"

#include <assert.h>

int
WorkshopJob::SetProgress(unsigned progress, const char *timeout)
{
    return queue.SetJobProgress(*this, progress, timeout);
}

bool
WorkshopJob::SetDone(int status)
{
    return queue.SetJobDone(*this, status);
}
