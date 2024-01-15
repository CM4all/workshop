// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CurlOperator.hxx"
#include "Workplace.hxx"
#include "CaptureBuffer.hxx"
#include "util/Exception.hxx"
#include "util/PrintException.hxx"
#include "util/SpanCast.hxx"
#include "util/StringCompare.hxx"

#include <algorithm>

using std::string_view_literals::operator""sv;

CronCurlOperator::CronCurlOperator(CronQueue &_queue,
				   CronWorkplace &_workplace,
				   CronJob &&_job,
				   std::string &&_start_time,
				   CurlGlobal &_global,
				   const char *url) noexcept
	:CronOperator(_queue, _workplace,
		      std::move(_job),
		      {},
		      std::move(_start_time)),
	 request(_global, url, *this)
{
}

CronCurlOperator::~CronCurlOperator() noexcept = default;

void
CronCurlOperator::Start()
{
	/* kill after 5 minutes */
	timeout_event.Schedule(std::chrono::minutes(5));

	request.Start();
}

void
CronCurlOperator::OnHeaders(HttpStatus _status, Curl::Headers &&headers)
{
	status = _status;
	(void)headers;

	const auto ct = headers.find("content-type");
	if (ct != headers.end()) {
		const char *content_type = ct->second.c_str();
		if (StringStartsWith(content_type, "text/"sv))
			/* capture the response body if it's text */
			output_capture = std::make_unique<CaptureBuffer>(8192);
	}
}

void
CronCurlOperator::OnData(std::span<const std::byte> _src)
{
	if (output_capture) {
		const auto src = ToStringView(_src);
		auto w = output_capture->Write();
		size_t nbytes = std::min(w.size(), src.size());
		std::copy_n(src.begin(), nbytes, w.begin());
		output_capture->Append(nbytes);
	}
}

void
CronCurlOperator::OnEnd()
{
	const char *log = output_capture
		? output_capture->NormalizeASCII()
		: nullptr;

	Finish(static_cast<int>(status), log);
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
