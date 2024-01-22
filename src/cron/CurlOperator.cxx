// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CurlOperator.hxx"
#include "Result.hxx"
#include "CaptureBuffer.hxx"
#include "util/PrintException.hxx"
#include "util/SpanCast.hxx"
#include "util/StringCompare.hxx"

#include <algorithm>

using std::string_view_literals::operator""sv;

CronCurlOperator::CronCurlOperator(LazyDomainLogger &_logger,
				   CurlGlobal &_global,
				   const char *url) noexcept
	:CronOperator(_logger),
	 request(_global, url, *this)
{
}

CronCurlOperator::~CronCurlOperator() noexcept = default;

void
CronCurlOperator::Start()
{
	request.Start();
}

void
CronCurlOperator::OnHeaders(HttpStatus _status, Curl::Headers &&headers)
{
	status = _status;

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
	CronResult result{
		.exit_status = static_cast<int>(status),
	};

	if (output_capture)
		result.log = std::move(*output_capture).NormalizeASCII();

	Finish(std::move(result));
}

void
CronCurlOperator::OnError(std::exception_ptr ep) noexcept
{
	PrintException(ep);

	Finish(CronResult::Error(ep));
}
