// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Library.hxx"
#include "Plan.hxx"
#include "StatxTimestamp.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "io/DirectoryReader.hxx"
#include "io/FileAt.hxx"
#include "io/FileName.hxx"
#include "io/Open.hxx"
#include "util/CharUtil.hxx"

#include <assert.h>
#include <fcntl.h> // for AT_*
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

Library::Library(const char *_path)
	:logger("library"),
	 path(_path),
	 directory_fd(OpenPath(_path, O_DIRECTORY)) {}

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

	DirectoryReader dr{OpenDirectory({directory_fd, "."})};
	while (const char *filename = dr.Read()) {
		if (IsSpecialFilename(filename) || !is_valid_plan_name(filename))
			continue;

		auto old_i = old_plans.find(filename);
		decltype(old_i) i;

		if (old_i != old_plans.end()) {
			i = plans.emplace(std::piecewise_construct,
					  std::forward_as_tuple(filename),
					  std::forward_as_tuple(std::move(old_i->second)))
				.first;
			old_plans.erase(old_i);
		} else {
			i = plans.emplace(std::piecewise_construct,
					  std::forward_as_tuple(filename),
					  std::forward_as_tuple())
				.first;
			modified = true;
		}

		if (UpdatePlan(filename, i->second, now))
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

	struct statx stx;
	if (statx(-1, path.c_str(), AT_STATX_FORCE_SYNC,
		  STATX_TYPE|STATX_MTIME,
		  &stx) < 0) {
		logger.Fmt(2, "Failed to stat {:?}: {}", path, strerror(errno));
		return false;
	}

	if (!S_ISDIR(stx.stx_mode)) {
		logger.Fmt(2, "not a directory: {}", path);
		return false;
	}

	const auto new_mtime = stx.stx_mtime;

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
