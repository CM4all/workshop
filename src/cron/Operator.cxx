// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Operator.hxx"

#include <fmt/core.h>

std::string
CronOperator::MakeLoggerDomain() const noexcept
{
	return fmt::format("cron job={} account={}", job.id, job.account_id);
}
