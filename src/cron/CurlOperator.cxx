/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "CurlOperator.hxx"
#include "Workplace.hxx"
#include "Queue.hxx"
#include "event/Duration.hxx"
#include "util/PrintException.hxx"

CronCurlOperator::CronCurlOperator(CronQueue &_queue,
                                   CronWorkplace &_workplace,
                                   CronJob &&_job,
                                   std::string &&_start_time,
                                   CurlGlobal &_global,
                                   const char *url) noexcept
    :CronOperator(_queue, _workplace,
                  std::move(_job),
                  std::move(_start_time)),
     request(_global, url, *this)
{
    /* kill after 5 minutes */
    timeout_event.Add(EventDuration<300>::value);
}

void
CronCurlOperator::Cancel()
{
    queue.Finish(job);
    queue.InsertResult(job, start_time.c_str(), -1, "Canceled");
    timeout_event.Cancel();
    workplace.OnExit(this);
}

void
CronCurlOperator::OnHeaders(unsigned _status,
                            std::multimap<std::string, std::string> &&headers)
{
    status = _status;
    (void)headers;
}

void
CronCurlOperator::OnData(ConstBuffer<void> data)
{
    (void)data;
}

void
CronCurlOperator::OnEnd()
{
    queue.Finish(job);
    // TODO: use response body?
    queue.InsertResult(job, start_time.c_str(), status, nullptr);
    timeout_event.Cancel();
    workplace.OnExit(this);
}

void
CronCurlOperator::OnError(std::exception_ptr ep)
{
    try {
        std::rethrow_exception(ep);
    } catch (const std::exception &e) {
        PrintException(e);
    } catch (...) {
    }

    queue.Finish(job);
    queue.InsertResult(job, start_time.c_str(), -1, nullptr);
    timeout_event.Cancel();
    workplace.OnExit(this);
}
