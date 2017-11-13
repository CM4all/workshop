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

void
WorkshopJob::SetDone(int status, const char *log)
{
    queue.SetJobDone(*this, status, log);
}

void
WorkshopJob::SetAgain()
{
    queue.RollbackJob(*this);
}
