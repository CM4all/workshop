/*
 * Manage the list of plans (= library) and load their configuration
 * files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Library.hxx"
#include "Plan.hxx"
#include "pg_array.hxx"

#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>

void
Library::UpdatePlanNames()
{
    const time_t now = time(NULL);

    if (!names.empty() && now < next_names_update)
        return;

    next_names_update = now + 60;

    /* collect new list */

    std::list<std::string> plan_names;

    for (const auto &i : plans) {
        const std::string &name = i.first;
        const PlanEntry &entry = i.second;

        if (!entry.IsDisabled(now))
            plan_names.push_back(name);
    }

    names = pg_encode_array(plan_names);
}
