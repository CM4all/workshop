// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Operator.hxx"
#include "Handler.hxx"
#include "Result.hxx"

#include <fmt/core.h>

using std::string_view_literals::operator""sv;

CronOperator::CronOperator(CronHandler &_handler,
			   CronJob &&_job) noexcept
	:handler(_handler), job(std::move(_job)),
	 logger(*this)
{
}

void
CronOperator::Finish(const CronResult &result) noexcept
{
	handler.OnFinish(result);
}

std::string
CronOperator::MakeLoggerDomain() const noexcept
{
	return fmt::format("cron job={} account={}", job.id, job.account_id);
}
