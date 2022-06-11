/*
 * Copyright 2006-2022 CM4all GmbH
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
#include "PlanLoader.hxx"
#include "util/Exception.hxx"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

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
	struct stat st;

	const auto plan_path = GetPath() / name;

	ret = stat(plan_path.c_str(), &st);
	if (ret < 0) {
		if (ret != ENOENT)
			logger(2, "failed to stat '", plan_path.c_str(), "': ",
			       strerror(errno));

		entry.Clear();

		return false;
	}

	if (!S_ISREG(st.st_mode)) {
		entry.Clear();

		DisablePlan(entry, now, std::chrono::seconds(60));
		return false;
	}

	// TODO: use st.st_mtime instead of doing another stat()?
	const auto new_mtime = std::filesystem::last_write_time(plan_path);
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

	ret = stat(plan->GetExecutablePath().c_str(), &st);
	if (ret < 0) {
		const int e = errno;
		if (e != ENOENT || !entry.deinstalled)
			logger(2, "failed to stat '", plan->GetExecutablePath().c_str(),
			       "': ", strerror(e));
		if (e == ENOENT)
			entry.deinstalled = true;
		else
			DisablePlan(entry, now, std::chrono::seconds(60));
		return false;
	}

	entry.deinstalled = false;

	return true;
}

bool
Library::LoadPlan(const char *name, PlanEntry &entry,
		  std::chrono::steady_clock::time_point now) noexcept
{
	assert(entry.plan == nullptr);

	logger(6, "loading plan '", name, "'");

	const auto plan_path = GetPath() / name;

	try {
		entry.plan.reset(new Plan(LoadPlanFile(plan_path)));
	} catch (...) {
		logger(1, "failed to load plan '", name, "': ",
		       std::current_exception());
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
