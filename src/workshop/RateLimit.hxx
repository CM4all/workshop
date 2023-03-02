// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <chrono>

struct RateLimit {
	using Duration = std::chrono::seconds;

	/**
	 * The interval in which the number of occurrences is counted.
	 */
	Duration duration{};

	/**
	 * The maximum number of occurrences of the described event
	 * within the given interval.
	 */
	unsigned max_count = 0;

	bool IsDefined() const noexcept {
		return duration > Duration{};
	}

	/**
	 * Throws on error.
	 */
	static RateLimit Parse(const char *s);
};
