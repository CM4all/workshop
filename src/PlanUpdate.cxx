/*
 * Manage the list of plans (= library) and load their configuration
 * files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Library.hxx"
#include "Plan.hxx"
#include "util/Error.hxx"

#include <daemon/log.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

static void
disable_plan(Library &library, PlanEntry &entry,
             std::chrono::steady_clock::duration duration)
{
    entry.Disable(std::chrono::steady_clock::now(), duration);
    library.next_names_update = std::chrono::steady_clock::time_point::min();
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
        entry.plan.reset();

        entry.mtime = 0;

        disable_plan(library, entry, std::chrono::seconds(60));
        return ENOENT;
    }

    if (st.st_mtime != entry.mtime) {
        entry.disabled_until = std::chrono::steady_clock::time_point::min();
        entry.plan.reset();

        entry.mtime = st.st_mtime;
    }

    if (entry.disabled_until > std::chrono::steady_clock::time_point::min()) {
        if (std::chrono::steady_clock::now() < entry.disabled_until)
            /* this plan is temporarily disabled due to previous errors */
            return ENOENT;

        entry.disabled_until = std::chrono::steady_clock::time_point::min();
    }

    return 0;
}

static int
validate_plan(Library &library, PlanEntry &entry)
{
    const auto &plan = entry.plan;
    int ret;
    struct stat st;

    assert(plan);
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
            disable_plan(library, entry, std::chrono::seconds(60));
        return ENOENT;
    }

    entry.deinstalled = false;

    return 0;
}

static bool
load_plan_entry(Library &library, PlanEntry &entry)
{
    char path[1024];

    assert(entry.plan == nullptr);
    assert(entry.mtime != 0);

    daemon_log(6, "loading plan '%s'\n", entry.name.c_str());

    snprintf(path, sizeof(path), "%s/%s",
             library.path.c_str(), entry.name.c_str());

    Plan plan;
    Error error;
    if (!plan.LoadFile(path, error)) {
        daemon_log(2, "failed to load plan '%s': %s\n",
                   entry.name.c_str(), error.GetMessage());
        disable_plan(library, entry, std::chrono::seconds(600));
        return false;
    }

    entry.plan.reset(new Plan(std::move(plan)));

    library.next_names_update = std::chrono::steady_clock::time_point::min();

    return true;
}

int
Library::UpdatePlan(PlanEntry &entry)
{
    int ret;

    ret = check_plan_mtime(*this, entry);
    if (ret != 0)
        return ret;

    if (entry.plan == nullptr && !load_plan_entry(*this, entry))
        return ret;

    ret = validate_plan(*this, entry);
    if (ret != 0)
        return ret;

    return 0;
}
