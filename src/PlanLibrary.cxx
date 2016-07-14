/*
 * Manage a plan library.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Library.hxx"
#include "Plan.hxx"

#include <daemon/log.h>

#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>

static int is_valid_plan_name_char(char ch) {
    return (ch >= 'a' && ch <= 'z') ||
        (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') ||
        ch == '_' || ch == '-';
}

static int is_valid_plan_name(const char *name) {
    assert(name != nullptr);

    do {
        if (!is_valid_plan_name_char(*name))
            return 0;
        ++name;
    } while (*name != 0);

    return 1;
}

int
Library::UpdatePlans()
{
    struct dirent *ent;

    /* read list of plans from file system, update our list */

    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) {
        fprintf(stderr, "failed to opendir '%s': %s\n",
                path.c_str(), strerror(errno));
        return -1;
    }

    ++generation;

    while ((ent = readdir(dir)) != nullptr) {
        if (!is_valid_plan_name(ent->d_name))
            continue;

        PlanEntry &entry = MakePlanEntry(ent->d_name);
        UpdatePlan(entry);
        entry.generation = generation;
    }

    closedir(dir);

    /* remove all plans */

    for (auto i = plans.begin(), end = plans.end(); i != end;) {
        PlanEntry &entry = i->second;
        if (entry.generation != generation) {
            daemon_log(3, "removed plan '%s'\n", i->first.c_str());

            i = plans.erase(i);
            next_names_update = 0;
        } else
            ++i;
    }

    return 0;
}

bool
Library::Update()
{
    const time_t now = time(nullptr);
    int ret;
    struct stat st;

    /* check directory time stamp */

    ret = stat(path.c_str(), &st);
    if (ret < 0) {
        fprintf(stderr, "failed to stat '%s': %s\n",
                path.c_str(), strerror(errno));
        return false;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "not a directory: %s\n", path.c_str());
        return false;
    }

    if (st.st_mtime == mtime && now < next_plans_check)
        return true;

    /* do it */

    ret = UpdatePlans();
    if (ret != 0)
        return false;

    /* update mtime */

    mtime = st.st_mtime;
    next_plans_check = now + 60;

    return true;
}

Plan *
Library::Get(const char *name)
{
    auto i = plans.find(name);
    if (i == plans.end())
        return nullptr;

    PlanEntry &entry = i->second;

    int ret = UpdatePlan(entry);
    if (ret != 0)
        return nullptr;

    ++entry.plan->ref;
    return entry.plan;
}

static bool
find_plan_pointer(const Library &library, const Plan *plan)
{
    for (const auto &i : library.plans)
        if (i.second.plan == plan)
            return true;

    return false;
}

void
plan_put(Plan **plan_r)
{
    Library *library;

    assert(plan_r != nullptr);
    assert(*plan_r != nullptr);

    Plan *plan = *plan_r;
    *plan_r = nullptr;

    library = plan->library;

    assert(plan->ref > 0);
    assert(library != nullptr);

    --plan->ref;

    if (plan->ref == 0) {
        /* free "old" plans which have refcount 0 */
        if (!find_plan_pointer(*library, plan))
            delete plan;
    }
}
