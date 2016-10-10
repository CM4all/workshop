/*
 * Manage the list of plans (= library) and load their configuration
 * files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Library.hxx"
#include "Plan.hxx"
#include "pg/Array.hxx"

std::string
Library::GetPlanNames() const
{
    /* collect new list */

    const auto now = std::chrono::steady_clock::now();

    std::list<std::string> plan_names;

    for (const auto &i : plans) {
        const std::string &name = i.first;
        const PlanEntry &entry = i.second;

        if (entry.IsAvailable(now))
            plan_names.push_back(name);
    }

    return pg_encode_array(plan_names);
}
