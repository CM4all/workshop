/*
 * Manage the list of plans (= library) and load their configuration
 * files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Library.hxx"
#include "Plan.hxx"
#include "pg/Array.hxx"

void
Library::UpdatePlanNames()
{
    const auto now = std::chrono::steady_clock::now();

    if (!names.empty() && now < next_names_update)
        return;

    next_names_update = now + std::chrono::seconds(60);

    /* collect new list */

    std::list<std::string> plan_names;

    for (const auto &i : plans) {
        const std::string &name = i.first;
        const PlanEntry &entry = i.second;

        if (entry.IsAvailable(now))
            plan_names.push_back(name);
    }

    names = pg_encode_array(plan_names);
}
