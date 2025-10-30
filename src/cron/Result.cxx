// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Result.hxx"
#include "util/Exception.hxx"

CronResult
CronResult::Error(std::exception_ptr error) noexcept
{
	return Error(GetFullMessage(error));
}
