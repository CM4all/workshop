// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Library.hxx"
#include "Plan.hxx"
#include "PlanLoader.hxx"
#include "StatxTimestamp.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "util/Exception.hxx"

#include <assert.h>
#include <fcntl.h> // for AT_*
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>

void
Library::DisablePlan(PlanEntry &entry,
		     std::chrono::steady_clock::time_point now,
		     std::chrono::steady_clock::duration duration) noexcept
{
	entry.Disable(now, duration);
}

bool
Library::CheckPlanModified(const char *name, PlanEntry &entry,
			   std::chrono::steady_clock::time_point now) noexcept
{
	int ret;

	struct statx stx;
	ret = statx(directory_fd.Get(), name, AT_STATX_FORCE_SYNC,
		    STATX_TYPE|STATX_MTIME,
		    &stx);
	if (ret < 0) {
		if (ret != ENOENT)
			logger.Fmt(2, "failed to stat {:?}/{:?}: {}",
				   path, name, strerror(errno));

		entry.Clear();

		return false;
	}

	if (!S_ISREG(stx.stx_mode)) {
		entry.Clear();

		DisablePlan(entry, now, std::chrono::seconds(60));
		return false;
	}

	// TODO: use st.st_mtime instead of doing another stat()?
	const auto new_mtime = stx.stx_mtime;
	if (new_mtime != entry.mtime) {
		entry.Enable();
		entry.plan.reset();

		entry.mtime = new_mtime;
	}

	if (entry.IsDisabled(now))
		/* this plan is temporarily disabled due to previous errors */
		return false;

	return true;
}

bool
Library::ValidatePlan(PlanEntry &entry,
		      std::chrono::steady_clock::time_point now) noexcept
{
	const auto &plan = entry.plan;
	int ret;
	struct stat st;

	assert(plan);

	/* check if the executable exists; it would not if the Debian
	   package has been deinstalled, but the plan's config file is
	   still there */

	if (!plan->translate) {
		ret = stat(plan->GetExecutablePath().c_str(), &st);
		if (ret < 0) {
			const int e = errno;
			if (e != ENOENT || !entry.deinstalled)
				logger.Fmt(2, "failed to stat {:?}: {}'",
					   plan->GetExecutablePath(), strerror(e));
			if (e == ENOENT)
				entry.deinstalled = true;
			else
				DisablePlan(entry, now, std::chrono::seconds(60));
			return false;
		}
	}

	entry.deinstalled = false;

	return true;
}

bool
Library::LoadPlan(const char *name, PlanEntry &entry,
		  std::chrono::steady_clock::time_point now) noexcept
{
	assert(entry.plan == nullptr);

	logger.Fmt(6, "loading plan {:?}", name);

	const auto plan_path = fmt::format("{}/{}", path, name);

	try {
		entry.plan.reset(new Plan(LoadPlanFile(plan_path)));
	} catch (...) {
		logger.Fmt(1, "failed to load plan {:?}: {}",
			   name, std::current_exception());
		DisablePlan(entry, now, std::chrono::seconds(600));
		return false;
	}

	return true;
}

bool
Library::UpdatePlan(const char *name, PlanEntry &entry,
		    std::chrono::steady_clock::time_point now) noexcept
{
	const bool was_available = entry.IsAvailable(now);

	if (!CheckPlanModified(name, entry, now))
		return entry.IsAvailable(now) != was_available;

	if (entry.plan == nullptr && !LoadPlan(name, entry, now))
		return false;

	return ValidatePlan(entry, now) != was_available;
}
