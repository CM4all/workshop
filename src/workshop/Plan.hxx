/*
 * Copyright 2006-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WORKSHOP_PLAN_HXX
#define WORKSHOP_PLAN_HXX

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
	std::vector<std::string> args;

	std::string timeout, chroot;

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

	bool control_channel = false;

	Plan() = default;

	Plan(Plan &&) = default;

	Plan(const Plan &other) = delete;

	Plan &operator=(Plan &&other) = default;
	Plan &operator=(const Plan &other) = delete;

	const std::string &GetExecutablePath() const {
		assert(!args.empty());

		return args.front();
	}
};

#endif
