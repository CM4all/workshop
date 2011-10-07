/*
 * Manage the list of plans (= library) and load their configuration
 * files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "plan-internal.h"
#include "plan.h"

#include <daemon/log.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

static void disable_plan(struct library *library,
                         struct plan_entry *entry,
                         time_t duration) {
    entry->disabled_until = time(NULL) + duration;
    library->next_names_update = 0;
}

static int check_plan_mtime(struct library *library, struct plan_entry *entry) {
    int ret;
    char path[1024];
    struct stat st;

    snprintf(path, sizeof(path), "%s/%s",
             library->path, entry->name);
    ret = stat(path, &st);
    if (ret < 0) {
        if (ret != ENOENT)
            fprintf(stderr, "failed to stat '%s': %s\n",
                    path, strerror(errno));

        entry->mtime = 0;

        return errno;
    }

    if (!S_ISREG(st.st_mode)) {
        if (entry->plan != NULL) {
            /* free memory of old plan only if there are no
               references on it anymore */
            if (entry->plan->ref == 0)
                plan_free(&entry->plan);
            else
                entry->plan = NULL;
        }

        entry->mtime = 0;

        disable_plan(library, entry, 60);
        return ENOENT;
    }

    if (st.st_mtime != entry->mtime) {
        entry->disabled_until = 0;

        if (entry->plan != NULL) {
            /* free memory of old plan only if there are no
               references on it anymore */
            if (entry->plan->ref == 0)
                plan_free(&entry->plan);
            else
                entry->plan = NULL;
        }

        entry->mtime = st.st_mtime;
    }

    if (entry->disabled_until > 0) {
        if (time(NULL) < entry->disabled_until)
            /* this plan is temporarily disabled due to previous errors */
            return ENOENT;

        entry->disabled_until = 0;
    }

    return 0;
}

static int validate_plan(struct plan_entry *entry) {
    const struct plan *plan = entry->plan;
    int ret;
    struct stat st;

    assert(plan != NULL);
    assert(plan->argv.num > 0);
    assert(plan->argv.values[0] != NULL && plan->argv.values[0][0] != 0);
    assert(plan->library != NULL);

    /* check if the executable exists; it would not if the Debian
       package has been deinstalled, but the plan's config file is
       still there */

    ret = stat(plan->argv.values[0], &st);
    if (ret < 0) {
        if (errno != ENOENT || !entry->deinstalled)
            fprintf(stderr, "failed to stat '%s': %s\n",
                    plan->argv.values[0], strerror(errno));
        if (errno == ENOENT)
            entry->deinstalled = 1;
        else
            disable_plan(plan->library, entry, 60);
        return ENOENT;
    }

    entry->deinstalled = 0;

    return 0;
}

static int load_plan_entry(struct library *library,
                           struct plan_entry *entry) {
    int ret;
    char path[1024];

    assert(entry->name != NULL);
    assert(entry->plan == NULL);
    assert(entry->mtime != 0);

    daemon_log(6, "loading plan '%s'\n", entry->name);

    snprintf(path, sizeof(path), "%s/%s",
             library->path, entry->name);

    ret = plan_load(path, &entry->plan);
    if (ret != 0) {
        disable_plan(library, entry, 600);
        return ret;
    }

    entry->plan->library = library;

    library->next_names_update = 0;

    return 0;
}

int library_update_plan(struct library *library, struct plan_entry *entry) {
    int ret;

    ret = check_plan_mtime(library, entry);
    if (ret != 0)
        return ret;

    if (entry->plan == NULL) {
        ret = load_plan_entry(library, entry);
        if (ret != 0)
            return ret;
    }

    ret = validate_plan(entry);
    if (ret != 0)
        return ret;

    return 0;
}
