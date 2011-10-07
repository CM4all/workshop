/*
 * Manage a plan library.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "plan-internal.h"
#include "plan.h"
#include "workshop.h"

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

static struct plan_entry *find_plan_by_name(struct library *library,
                                            const char *name) {
    unsigned i;

    for (i = 0; i < library->num_plans; ++i)
        if (strcmp(library->plans[i].name, name) == 0)
            return &library->plans[i];

    return NULL;
}

static struct plan_entry *add_plan_entry(struct library *library,
                                         const char *name) {
    struct plan_entry *entry;

    assert(is_valid_plan_name(name));

    if (library->num_plans >= library->max_plans) {
        library->max_plans += 16;
        library->plans = realloc(library->plans,
                                 library->max_plans * sizeof(library->plans[0]));
        if (library->plans == NULL)
            abort();
    }

    entry = &library->plans[library->num_plans++];
    memset(entry, 0, sizeof(*entry));
    entry->name = strdup(name);
    if (entry->name == NULL)
        abort();

    return entry;
}

static struct plan_entry *make_plan_entry(struct library *library, const char *name) {
    struct plan_entry *entry;

    entry = find_plan_by_name(library, name);
    if (entry != NULL)
        return entry;

    return add_plan_entry(library, name);
}

static void library_remove_plan(struct library *library, unsigned i) {
    struct plan_entry *entry;

    assert(i < library->num_plans);

    entry = &library->plans[i];

    if (entry->name != NULL)
        free(entry->name);

    if (entry->plan != NULL && entry->plan->ref == 0)
        plan_free(&entry->plan);

    --library->num_plans;
    memmove(entry, entry + 1,
            sizeof(*entry) * (library->num_plans - i));
}

static int library_update_plans(struct library *library) {
    DIR *dir;
    struct dirent *ent;
    unsigned i;

    /* read list of plans from file system, update our list */

    dir = opendir(library->path);
    if (dir == NULL) {
        fprintf(stderr, "failed to opendir '%s': %s\n",
                library->path, strerror(errno));
        return -1;
    }

    ++library->generation;

    while ((ent = readdir(dir)) != NULL) {
        struct plan_entry *entry;

        if (!is_valid_plan_name(ent->d_name))
            continue;

        entry = make_plan_entry(library, ent->d_name);
        assert(entry != NULL);

        library_update_plan(library, entry);
        entry->generation = library->generation;
    }

    closedir(dir);

    /* remove all plans */

    for (i = library->num_plans; i > 0;) {
        struct plan_entry *entry = &library->plans[--i];

        if (entry->generation != library->generation) {
            log(3, "removed plan '%s'\n", entry->name);

            library_remove_plan(library, i);
            library->next_names_update = 0;
        }
    }

    return 0;
}

int library_update(struct library *library) {
    const time_t now = time(NULL);
    int ret;
    struct stat st;

    /* check directory time stamp */

    ret = stat(library->path, &st);
    if (ret < 0) {
        fprintf(stderr, "failed to stat '%s': %s\n",
                library->path, strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "not a directory: %s\n", library->path);
        return -1;
    }

    if (st.st_mtime == library->mtime && now < library->next_plans_check)
        return 0;

    /* do it */

    ret = library_update_plans(library);
    if (ret != 0)
        return ret;
        
    /* update mtime */

    library->mtime = st.st_mtime;
    library->next_plans_check = now + 60;

    return 0;
}

int library_get(struct library *library, const char *name,
                struct plan **plan_r) {
    int ret;
    struct plan_entry *entry;

    entry = find_plan_by_name(library, name);
    if (entry == NULL)
        return ENOENT;

    ret = library_update_plan(library, entry);
    if (ret != 0)
        return ret;

    *plan_r = entry->plan;
    ++entry->plan->ref;
    ++library->ref;
    return 0;
}

static int find_plan_index(struct library *library,
                           const struct plan *plan) {
    unsigned i;

    for (i = 0; i < library->num_plans; ++i)
        if (library->plans[i].plan == plan)
            return (int)i;

    return -1;
}

void plan_put(struct plan **plan_r) {
    struct plan *plan;
    struct library *library;

    assert(plan_r != NULL);
    assert(*plan_r != NULL);

    plan = *plan_r;
    *plan_r = NULL;

    library = plan->library;

    assert(plan->ref > 0);
    assert(library != NULL);
    assert(library->ref > 0);

    --plan->ref;
    --library->ref;

    if (plan->ref == 0) {
        /* free "old" plans which have refcount 0 */
        int i = find_plan_index(library, plan);
        if (i < 0)
            plan_free(&plan);
    }
}
