/*
 * Copyright 2006-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "CurlOperator.hxx"
#include "Workplace.hxx"
#include "CaptureBuffer.hxx"
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
    timeout_event.Schedule(std::chrono::minutes(5));

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
            output_capture = std::make_unique<CaptureBuffer>(8192);
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
CronCurlOperator::OnError(std::exception_ptr ep) noexcept
{
    PrintException(ep);

    Finish(-1, GetFullMessage(ep).c_str());
    timeout_event.Cancel();
    workplace.OnExit(this);
}
