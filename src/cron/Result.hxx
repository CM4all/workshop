// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/AllocatedString.hxx"

#include <exception>

struct CronResult {
	AllocatedString log;

	int exit_status;

	[[gnu::pure]]
	static CronResult Error(std::string_view _log) noexcept {
		return {
			.log = AllocatedString{_log},
			.exit_status = -1,
		};
	}

	[[gnu::pure]]
	static CronResult Error(std::exception_ptr error) noexcept;
};
