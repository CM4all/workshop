/*
 * Manage the list of plans (= library) and load their configuration
 * files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "PlanInternal.hxx"
#include "Plan.hxx"
#include "Library.hxx"

#include <daemon/log.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

static void
disable_plan(Library &library, PlanEntry &entry, time_t duration)
{
    entry.Disable(time(NULL), duration);
    library.next_names_update = 0;
}

static int
check_plan_mtime(Library &library, PlanEntry &entry)
{
    int ret;
    char path[1024];
    struct stat st;

    snprintf(path, sizeof(path), "%s/%s",
             library.path.c_str(), entry.name.c_str());
    ret = stat(path, &st);
    if (ret < 0) {
        if (ret != ENOENT)
            fprintf(stderr, "failed to stat '%s': %s\n",
                    path, strerror(errno));

        entry.mtime = 0;

        return errno;
    }

    if (!S_ISREG(st.st_mode)) {
        if (entry.plan != NULL) {
            /* free memory of old plan only if there are no
               references on it anymore */
            if (entry.plan->ref == 0)
                delete entry.plan;
            entry.plan = NULL;
        }

        entry.mtime = 0;

        disable_plan(library, entry, 60);
        return ENOENT;
    }

    if (st.st_mtime != entry.mtime) {
        entry.disabled_until = 0;

        if (entry.plan != NULL) {
            /* free memory of old plan only if there are no
               references on it anymore */
            if (entry.plan->ref == 0)
                delete entry.plan;
            entry.plan = NULL;
        }

        entry.mtime = st.st_mtime;
    }

    if (entry.disabled_until > 0) {
        if (time(NULL) < entry.disabled_until)
            /* this plan is temporarily disabled due to previous errors */
            return ENOENT;

        entry.disabled_until = 0;
    }

    return 0;
}

static int
validate_plan(Library &library, PlanEntry &entry)
{
    const Plan *plan = entry.plan;
    int ret;
    struct stat st;

    assert(plan != NULL);
    assert(!plan->args.empty());
    assert(!plan->args.front().empty());

    /* check if the executable exists; it would not if the Debian
       package has been deinstalled, but the plan's config file is
       still there */

    ret = stat(plan->args.front().c_str(), &st);
    if (ret < 0) {
        if (errno != ENOENT || !entry.deinstalled)
            fprintf(stderr, "failed to stat '%s': %s\n",
                    plan->args.front().c_str(), strerror(errno));
        if (errno == ENOENT)
            entry.deinstalled = true;
        else
            disable_plan(library, entry, 60);
        return ENOENT;
    }

    entry.deinstalled = false;

    return 0;
}

static bool
load_plan_entry(Library &library, PlanEntry &entry)
{
    char path[1024];

    assert(entry.plan == NULL);
    assert(entry.mtime != 0);

    daemon_log(6, "loading plan '%s'\n", entry.name.c_str());

    snprintf(path, sizeof(path), "%s/%s",
             library.path.c_str(), entry.name.c_str());

    entry.plan = plan_load(path);
    if (entry.plan == nullptr) {
        disable_plan(library, entry, 600);
        return false;
    }

    entry.plan->library = &library;

    library.next_names_update = 0;

    return true;
}

int
Library::UpdatePlan(PlanEntry &entry)
{
    int ret;

    ret = check_plan_mtime(*this, entry);
    if (ret != 0)
        return ret;

    if (entry.plan == NULL && !load_plan_entry(*this, entry))
        return ret;

    ret = validate_plan(*this, entry);
    if (ret != 0)
        return ret;

    return 0;
}
