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

#include "Hook.hxx"
#include "workshop/MultiLibrary.hxx"
#include "workshop/Plan.hxx"
#include "spawn/Prepared.hxx"
#include "util/RuntimeError.hxx"

static bool
CompareGroups(const std::vector<gid_t> &a, const std::array<gid_t, 32> &b)
{
	if (a.size() > b.size())
		return false;

	if (a.size() < b.size() && b[a.size()] != 0)
		return false;

	for (size_t i = 0; i < a.size(); ++i)
		if (a[i] != b[i])
			return false;

	return true;
}

bool
WorkshopSpawnHook::Verify(const PreparedChildProcess &p)
{
	if (p.hook_info != nullptr && library) {
		const auto now = std::chrono::steady_clock::now();

		library->Update(now, false);

		const char *plan_name = p.hook_info;
		auto plan = library->Get(now, plan_name);
		if (!plan)
			throw FormatRuntimeError("No such plan: %s", plan_name);

		if (p.uid_gid.uid != plan->uid)
			throw FormatRuntimeError("Wrong uid %d, expected %d for plan %s",
						 int(p.uid_gid.uid), int(plan->uid),
						 plan_name);

		if (p.uid_gid.gid != plan->gid)
			throw FormatRuntimeError("Wrong gid %d, expected %d for plan %s",
						 int(p.uid_gid.gid), int(plan->gid),
						 plan_name);

		if (!CompareGroups(plan->groups, p.uid_gid.groups))
			throw FormatRuntimeError("Supplementary group mismatch for plan %s",
						 plan_name);

		if (p.args.empty() || plan->args.empty() ||
		    plan->args.front() != p.args.front())
			throw FormatRuntimeError("Executable mismatch for plan %s",
						 plan_name);

		return true;
	} else
		return false;
}
