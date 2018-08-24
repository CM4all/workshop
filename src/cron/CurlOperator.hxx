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

#ifndef CRON_CURL_OPERATOR_HXX
#define CRON_CURL_OPERATOR_HXX

#include "Operator.hxx"
#include "curl/Handler.hxx"
#include "curl/Request.hxx"

#include <memory>

class CaptureBuffer;

/**
 * A #CronJob which sends a HTTP GET request to a specific URL.
 */
class CronCurlOperator final
    : public CronOperator, CurlResponseHandler {

    CurlRequest request;

    unsigned status = 0;

    std::unique_ptr<CaptureBuffer> output_capture;

public:
    CronCurlOperator(CronQueue &_queue, CronWorkplace &_workplace,
                     CronJob &&_job,
                     std::string &&_start_time,
                     CurlGlobal &_global,
                     const char *url) noexcept;
    ~CronCurlOperator() override;

    void Start();

    void Cancel() override;

private:
    /* virtual methods from CurlResponseHandler */
    void OnHeaders(unsigned status,
                   std::multimap<std::string, std::string> &&headers) override;
    void OnData(ConstBuffer<void> data) override;
    void OnEnd() override;
    void OnError(std::exception_ptr ep) noexcept override;
};

#endif
