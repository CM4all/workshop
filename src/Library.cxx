/*
 * Manage the list of plans (= library) and load their configuration
 * files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Library.hxx"
#include "Plan.hxx"
#include "PlanInternal.hxx"
#include "pg_array.hxx"

#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>

Library *
Library::Open(const char *path)
{
    int ret;
    struct stat st;

    assert(path != NULL);

    /* check path */

    ret = stat(path, &st);
    if (ret < 0) {
        fprintf(stderr, "failed to stat '%s': %s\n",
                path, strerror(errno));
        return NULL;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "not a directory: %s\n", path);
        return NULL;
    }

    /* create library object */

    return new Library(path);
}

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
