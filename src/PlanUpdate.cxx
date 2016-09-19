/*
 * Manage the list of plans (= library) and load their configuration
 * files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Library.hxx"
#include "Plan.hxx"
#include "PlanLoader.hxx"
#include "util/PrintException.hxx"

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
             std::chrono::steady_clock::time_point now,
             std::chrono::steady_clock::duration duration)
{
    entry.Disable(now, duration);
    library.ScheduleNamesUpdate();
}

static bool
check_plan_mtime(Library &library, const char *name, PlanEntry &entry,
                 std::chrono::steady_clock::time_point now)
{
    int ret;
    struct stat st;

    const auto path = library.GetPath() / name;

    ret = stat(path.c_str(), &st);
    if (ret < 0) {
        if (ret != ENOENT)
            fprintf(stderr, "failed to stat '%s': %s\n",
                    path.c_str(), strerror(errno));

        entry.Clear();

        return false;
    }

    if (!S_ISREG(st.st_mode)) {
        entry.Clear();

        disable_plan(library, entry, now, std::chrono::seconds(60));
        return false;
    }

    if (st.st_mtime != entry.mtime) {
        entry.disabled_until = std::chrono::steady_clock::time_point::min();
        entry.plan.reset();

        entry.mtime = st.st_mtime;
    }

    if (entry.IsDisabled(now))
        /* this plan is temporarily disabled due to previous errors */
        return false;

    return true;
}

static bool
validate_plan(Library &library, PlanEntry &entry,
              std::chrono::steady_clock::time_point now)
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
            disable_plan(library, entry, now, std::chrono::seconds(60));
        return false;
    }

    entry.deinstalled = false;

    return true;
}

static bool
load_plan_entry(Library &library, const char *name, PlanEntry &entry,
                std::chrono::steady_clock::time_point now)
{
    assert(entry.plan == nullptr);
    assert(entry.mtime != 0);

    daemon_log(6, "loading plan '%s'\n", name);

    const auto path = library.GetPath() / name;

    try {
        entry.plan.reset(new Plan(LoadPlanFile(path)));
    } catch (const std::runtime_error &e) {
        daemon_log(2, "failed to load plan '%s': %s\n",
                   name, e.what());
        PrintException(e);
        disable_plan(library, entry, now, std::chrono::seconds(600));
        return false;
    }

    library.ScheduleNamesUpdate();

    return true;
}

void
Library::UpdatePlan(const char *name, PlanEntry &entry,
                    std::chrono::steady_clock::time_point now)
{
    if (!check_plan_mtime(*this, name, entry, now))
        return;

    if (entry.plan == nullptr && !load_plan_entry(*this, name, entry, now))
        return;

    validate_plan(*this, entry, now);
}
