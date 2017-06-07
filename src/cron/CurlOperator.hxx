/*
 * author: Max Kellermann <mk@cm4all.com>
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
    void OnError(std::exception_ptr ep) override;
};

#endif
