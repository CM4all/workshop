/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
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
Instance::Verify(const PreparedChildProcess &p)
{
    if (p.hook_info != nullptr && library) {
        library->Update(false);

        const char *plan_name = p.hook_info;
        auto plan = library->Get(plan_name);
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
