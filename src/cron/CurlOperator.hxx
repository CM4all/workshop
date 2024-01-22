// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef CRON_CURL_OPERATOR_HXX
#define CRON_CURL_OPERATOR_HXX

#include "Operator.hxx"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Request.hxx"

#include <memory>

class CaptureBuffer;

/**
 * A #CronJob which sends a HTTP GET request to a specific URL.
 */
class CronCurlOperator final
	: public CronOperator, CurlResponseHandler
{
	CurlRequest request;

	HttpStatus status{};

	std::unique_ptr<CaptureBuffer> output_capture;

public:
	CronCurlOperator(CronJob &&_job,
			 CurlGlobal &_global,
			 const char *url) noexcept;
	~CronCurlOperator() noexcept override;

	void Start();

private:
	/* virtual methods from CurlResponseHandler */
	void OnHeaders(HttpStatus status, Curl::Headers &&headers) override;
	void OnData(std::span<const std::byte> src) override;
	void OnEnd() override;
	void OnError(std::exception_ptr ep) noexcept override;
};

#endif
