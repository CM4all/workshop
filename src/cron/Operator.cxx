// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Operator.hxx"
#include "Handler.hxx"
#include "Result.hxx"

#include <fmt/core.h>

using std::string_view_literals::operator""sv;

CronOperator::CronOperator(EventLoop &event_loop, CronHandler &_handler,
			   CronJob &&_job,
			   std::string_view _tag) noexcept
	:handler(_handler), job(std::move(_job)),
	 logger(*this),
	 tag(_tag),
	 timeout_event(event_loop, BIND_THIS_METHOD(OnTimeout))
{
	/* kill after the timeout expires */
	timeout_event.Schedule(job.timeout);
}

void
CronOperator::Finish(const CronResult &result) noexcept
{
	handler.OnFinish(result);
}

void
CronOperator::InvokeExit() noexcept
{
	handler.OnExit();
}

void
CronOperator::OnTimeout() noexcept
{
	logger(2, "Timeout");

	Finish(CronResult::Error("Timeout"sv));
	InvokeExit();
}

std::string
CronOperator::MakeLoggerDomain() const noexcept
{
	return fmt::format("cron job={} account={}", job.id, job.account_id);
}
