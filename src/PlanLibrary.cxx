/*
 * Manage a plan library.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Library.hxx"
#include "PlanInternal.hxx"
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
    assert(name != NULL);

    do {
        if (!is_valid_plan_name_char(*name))
            return 0;
        ++name;
    } while (*name != 0);

    return 1;
}

static PlanEntry *
find_plan_by_name(Library &library, const char *name)
{
    auto i = library.plans.find(name);
    return i != library.plans.end()
        ? &i->second
        : NULL;
}

static PlanEntry &
make_plan_entry(Library &library, const char *name)
{
    return library.plans.insert(std::make_pair(name, PlanEntry(name)))
        .first->second;
}

static int
library_update_plans(Library &library) {
    struct dirent *ent;

    /* read list of plans from file system, update our list */

    DIR *dir = opendir(library.path.c_str());
    if (dir == NULL) {
        fprintf(stderr, "failed to opendir '%s': %s\n",
                library.path.c_str(), strerror(errno));
        return -1;
    }

    ++library.generation;

    while ((ent = readdir(dir)) != NULL) {
        if (!is_valid_plan_name(ent->d_name))
            continue;

        PlanEntry &entry = make_plan_entry(library, ent->d_name);
        library_update_plan(library, entry);
        entry.generation = library.generation;
    }

    closedir(dir);

    /* remove all plans */

    for (auto n = library.plans.begin(), end = library.plans.end();
         n != end;) {
        auto i = n;
        ++n;

        PlanEntry &entry = i->second;
        if (entry.generation != library.generation) {
            daemon_log(3, "removed plan '%s'\n", i->first.c_str());

            library.plans.erase(i);
            library.next_names_update = 0;
        }
    }

    return 0;
}

int
library_update(Library *library)
{
    const time_t now = time(NULL);
    int ret;
    struct stat st;

    /* check directory time stamp */

    ret = stat(library->path.c_str(), &st);
    if (ret < 0) {
        fprintf(stderr, "failed to stat '%s': %s\n",
                library->path.c_str(), strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "not a directory: %s\n", library->path.c_str());
        return -1;
    }

    if (st.st_mtime == library->mtime && now < library->next_plans_check)
        return 0;

    /* do it */

    ret = library_update_plans(*library);
    if (ret != 0)
        return ret;
        
    /* update mtime */

    library->mtime = st.st_mtime;
    library->next_plans_check = now + 60;

    return 0;
}

Plan *
library_get(Library *library, const char *name)
{
    PlanEntry *entry;

    entry = find_plan_by_name(*library, name);
    if (entry == NULL)
        return NULL;

    int ret = library_update_plan(*library, *entry);
    if (ret != 0)
        return NULL;

    ++entry->plan->ref;
    ++library->ref;
    return entry->plan;
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

    assert(plan_r != NULL);
    assert(*plan_r != NULL);

    Plan *plan = *plan_r;
    *plan_r = NULL;

    library = plan->library;

    assert(plan->ref > 0);
    assert(library != NULL);
    assert(library->ref > 0);

    --plan->ref;
    --library->ref;

    if (plan->ref == 0) {
        /* free "old" plans which have refcount 0 */
        if (!find_plan_pointer(*library, plan))
            delete plan;
    }
}
