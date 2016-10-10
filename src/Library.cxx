/*
 * Manage the list of plans (= library) and load their configuration
 * files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Library.hxx"
#include "Plan.hxx"
#include "pg/Array.hxx"

#include <forward_list>

std::string
Library::GetPlanNames(std::chrono::steady_clock::time_point now) const
{
    std::forward_list<std::string> plan_names;

    for (const auto &i : plans) {
        const std::string &name = i.first;
        const PlanEntry &entry = i.second;

        if (entry.IsAvailable(now))
            plan_names.emplace_front(name);
    }

    return pg_encode_array(plan_names);
}
