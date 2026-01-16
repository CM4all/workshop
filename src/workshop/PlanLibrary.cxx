// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Library.hxx"
#include "Plan.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "util/CharUtil.hxx"

#include <fmt/std.h>

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

static constexpr bool
is_valid_plan_name_char(char ch) noexcept
{
	return IsAlphaNumericASCII(ch) ||
		ch == '_' || ch == '-';
}

[[gnu::pure]]
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

inline bool
Library::UpdatePlans(std::chrono::steady_clock::time_point now)
{
	/* read list of plans from file system, update our list */

	auto old_plans = std::move(plans);
	plans.clear();

	bool modified = false;

	for (const auto &entry : std::filesystem::directory_iterator(path)) {
		const auto name = entry.path().filename();
		if (!is_valid_plan_name(name.c_str()))
			continue;

		auto old_i = old_plans.find(name.native());
		decltype(old_i) i;

		if (old_i != old_plans.end()) {
			i = plans.emplace(std::piecewise_construct,
					  std::forward_as_tuple(name.native()),
					  std::forward_as_tuple(std::move(old_i->second)))
				.first;
			old_plans.erase(old_i);
		} else {
			i = plans.emplace(std::piecewise_construct,
					  std::forward_as_tuple(name.native()),
					  std::forward_as_tuple())
				.first;
			modified = true;
		}

		if (UpdatePlan(name.c_str(), i->second, now))
			modified = true;
	}

	/* remove all plans */

	for (const auto &i : old_plans)
		logger.Fmt(3, "removed plan {:?}", i.first);

	if (!old_plans.empty()) {
		modified = true;
	}

	return modified;
}

bool
Library::Update(std::chrono::steady_clock::time_point now, bool force) noexcept
try {
	/* check directory time stamp */

	if (!std::filesystem::is_directory(path)) {
		logger.Fmt(2, "not a directory: {}", path);
		return false;
	}

	const auto new_mtime = std::filesystem::last_write_time(path);

	if (!force && new_mtime == mtime && now < next_plans_check)
		return false;

	/* do it */

	bool modified = UpdatePlans(now);

	/* update mtime */

	mtime = new_mtime;
	next_plans_check = now + std::chrono::seconds(60);

	return modified;
} catch (...) {
	logger.Fmt(2, "Failed to load plans from {:?}: {}",
		   path, std::current_exception());
	return false;
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
