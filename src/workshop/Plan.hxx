// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "RateLimit.hxx"
#include "spawn/ResourceLimits.hxx"

#include <string>
#include <vector>
#include <chrono>

#include <assert.h>
#include <sys/types.h>
#include <stdlib.h>

class Error;

/** a plan describes how to perform a specific job */
struct Plan {
	enum class Type {
		EXEC,
		TRANSLATE,
	};

	/**
	 * If non-empty, then this plan executes a process with these
	 * command-line arguments; the first string is the path of the
	 * executable.
	 */
	std::vector<std::string> args;

	std::string timeout, chroot;

	std::string reap_finished;

	std::chrono::steady_clock::duration parsed_timeout{};

	std::vector<RateLimit> rate_limits;

	uid_t uid = 65534;
	gid_t gid = 65534;

	/** supplementary group ids */
	std::vector<gid_t> groups;

	int umask = -1;

	ResourceLimits rlimits;

	int priority = 10;

	/** maximum concurrency for this plan */
	unsigned concurrency = 0;

	bool sched_idle = false, ioprio_idle = false;

	bool private_network = false;

	bool private_tmp = false;

	bool control_channel = false;

	bool allow_spawn = false;

	bool translate = false;

	bool notify_progress = false;

	Plan() = default;

	Plan(Plan &&) = default;

	Plan(const Plan &other) = delete;

	Plan &operator=(Plan &&other) = default;
	Plan &operator=(const Plan &other) = delete;

	[[gnu::pure]]
	Type GetType() const noexcept {
		if (translate)
			return Type::TRANSLATE;

		assert(!args.empty());

		return Type::EXEC;
	}

	const std::string &GetExecutablePath() const {
		assert(!args.empty());

		return args.front();
	}
};
