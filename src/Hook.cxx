// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Hook.hxx"
#include "workshop/MultiLibrary.hxx"
#include "workshop/Plan.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "spawn/Prepared.hxx"

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
			throw FmtRuntimeError("No such plan: {}", plan_name);

		if (p.uid_gid.real_uid != UidGid::UNSET_UID ||
		    p.uid_gid.real_gid != UidGid::UNSET_GID)
			throw std::runtime_error{"Real uid/gid not supported for plans"};

		if (p.uid_gid.effective_uid != plan->uid)
			throw FmtRuntimeError("Wrong uid {}, expected {} for plan {}",
					      p.uid_gid.effective_uid, plan->uid, plan_name);

		if (p.uid_gid.effective_gid != plan->gid)
			throw FmtRuntimeError("Wrong gid {}, expected {} for plan {}",
					      p.uid_gid.effective_gid, plan->gid, plan_name);

		if (!CompareGroups(plan->groups, p.uid_gid.supplementary_groups))
			throw FmtRuntimeError("Supplementary group mismatch for plan {}",
					      plan_name);

		if (p.args.empty() || plan->args.empty() ||
		    plan->args.front() != p.args.front())
			throw FmtRuntimeError("Executable mismatch for plan {}",
					      plan_name);

		return true;
	} else
		return false;
}
