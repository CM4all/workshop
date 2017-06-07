/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "CurlOperator.hxx"
#include "Workplace.hxx"
#include "CaptureBuffer.hxx"
#include "event/Duration.hxx"
#include "util/Exception.hxx"
#include "util/PrintException.hxx"

#include <string.h>

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
}

CronCurlOperator::~CronCurlOperator()
{
}

void
CronCurlOperator::Start()
{
    /* kill after 5 minutes */
    timeout_event.Add(EventDuration<300>::value);

    request.Start();
}

void
CronCurlOperator::Cancel()
{
    Finish(-1, "Canceled");
    timeout_event.Cancel();
    workplace.OnExit(this);
}

void
CronCurlOperator::OnHeaders(unsigned _status,
                            std::multimap<std::string, std::string> &&headers)
{
    status = _status;
    (void)headers;

    const auto ct = headers.find("content-type");
    if (ct != headers.end()) {
        const char *content_type = ct->second.c_str();
        if (strncmp(content_type, "text/", 5) == 0)
            /* capture the response body if it's text */
            output_capture = std::make_unique<CaptureBuffer>();
    }
}

void
CronCurlOperator::OnData(ConstBuffer<void> data)
{
    if (output_capture) {
        auto w = output_capture->Write();
        size_t nbytes = std::min(w.size, data.size);
        memcpy(w.data, data.data, nbytes);
        output_capture->Append(nbytes);
    }
}

void
CronCurlOperator::OnEnd()
{
    const char *log = output_capture
        ? output_capture->NormalizeASCII()
        : nullptr;

    Finish(status, log);
    timeout_event.Cancel();
    workplace.OnExit(this);
}

void
CronCurlOperator::OnError(std::exception_ptr ep)
{
    PrintException(ep);

    Finish(-1, GetFullMessage(ep).c_str());
    timeout_event.Cancel();
    workplace.OnExit(this);
}
