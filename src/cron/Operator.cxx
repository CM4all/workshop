/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Operator.hxx"
#include "Queue.hxx"

#include <daemon/log.h>

#include <unistd.h>
#include <sys/wait.h>

CronOperator::CronOperator(CronQueue &_queue, CronWorkplace &_workplace,
                           CronJob &&_job,
                           std::string &&_start_time) noexcept
    :queue(_queue), workplace(_workplace), job(std::move(_job)),
     start_time(std::move(_start_time)),
     timeout_event(GetEventLoop(), BIND_THIS_METHOD(OnTimeout))
{
}

EventLoop &
CronOperator::GetEventLoop()
{
    return queue.GetEventLoop();
}

void
CronOperator::Finish(int exit_status, const char *log)
{
    queue.Finish(job);
    queue.InsertResult(job, start_time.c_str(), exit_status, log);
}

void
CronOperator::OnTimeout()
{
    daemon_log(2, "Timeout on job %s\n", job.id.c_str());

    Cancel();
}
