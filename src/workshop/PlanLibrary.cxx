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

#include "Library.hxx"
#include "Plan.hxx"
#include "util/CharUtil.hxx"

#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>

static constexpr bool
is_valid_plan_name_char(char ch) noexcept
{
	return IsAlphaNumericASCII(ch) ||
		ch == '_' || ch == '-';
}

gcc_pure
static bool
is_valid_plan_name(const char *name) noexcept
{
	assert(name != nullptr);

	do {
		if (!is_valid_plan_name_char(*name))
			return false;
		++name;
	} while (*name != 0);

	return true;
}

bool
Library::UpdatePlans(std::chrono::steady_clock::time_point now) noexcept
{
	struct dirent *ent;

	/* read list of plans from file system, update our list */

	DIR *dir = opendir(path.c_str());
	if (dir == nullptr) {
		logger(2, "failed to opendir '", path.c_str(), "': ", strerror(errno));
		return false;
	}

	auto old_plans = std::move(plans);
	plans.clear();

	bool modified = false;

	while ((ent = readdir(dir)) != nullptr) {
		if (!is_valid_plan_name(ent->d_name))
			continue;

		std::string name(ent->d_name);

		auto old_i = old_plans.find(name);
		decltype(old_i) i;

		if (old_i != old_plans.end()) {
			i = plans.emplace(std::piecewise_construct,
					  std::forward_as_tuple(std::move(name)),
					  std::forward_as_tuple(std::move(old_i->second)))
				.first;
			old_plans.erase(old_i);
		} else {
			i = plans.emplace(std::piecewise_construct,
					  std::forward_as_tuple(std::move(name)),
					  std::forward_as_tuple())
				.first;
			modified = true;
		}

		if (UpdatePlan(ent->d_name, i->second, now))
			modified = true;
	}

	closedir(dir);

	/* remove all plans */

	for (const auto &i : old_plans)
		logger(3, "removed plan '", i.first, "'");

	if (!old_plans.empty()) {
		modified = true;
	}

	return modified;
}

bool
Library::Update(std::chrono::steady_clock::time_point now, bool force) noexcept
{
	int ret;
	struct stat st;

	/* check directory time stamp */

	ret = stat(path.c_str(), &st);
	if (ret < 0) {
		logger(2, "failed to stat '", path.c_str(), "': ", strerror(errno));
		return false;
	}

	if (!S_ISDIR(st.st_mode)) {
		logger(2, "not a directory: ", path.c_str());
		return false;
	}

	if (!force && st.st_mtime == mtime && now < next_plans_check)
		return false;

	/* do it */

	bool modified = UpdatePlans(now);

	/* update mtime */

	mtime = st.st_mtime;
	next_plans_check = now + std::chrono::seconds(60);

	return modified;
}

std::shared_ptr<Plan>
Library::Get(std::chrono::steady_clock::time_point now,
	     const char *name) noexcept
{
	auto i = plans.find(name);
	if (i == plans.end())
		return nullptr;

	PlanEntry &entry = i->second;

	UpdatePlan(name, entry, now);
	if (!entry.IsAvailable(now))
		return nullptr;

	return entry.plan;
}
