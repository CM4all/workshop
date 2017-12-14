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
WorkshopJob::SetEnv(const char *more_env)
{
    queue.SetJobEnv(*this, more_env);
}

void
WorkshopJob::SetDone(int status, const char *log)
{
    queue.SetJobDone(*this, status, log);
}

void
WorkshopJob::SetAgain(std::chrono::seconds delay)
{
    queue.AgainJob(*this, delay);
}
