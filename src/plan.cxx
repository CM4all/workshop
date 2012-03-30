/*
 * Manage the list of plans (= library) and load their configuration
 * files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "plan.hxx"
#include "plan_internal.hxx"

extern "C" {
#include "pg-util.h"
}

#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>

int library_open(const char *path, struct library **library_r) {
    int ret;
    struct stat st;
    struct library *library;

    assert(path != NULL);

    /* check path */

    ret = stat(path, &st);
    if (ret < 0) {
        fprintf(stderr, "failed to stat '%s': %s\n",
                path, strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "not a directory: %s\n", path);
        return -1;
    }

    /* create library object */

    library = (struct library *)calloc(1, sizeof(*library));
    if (library == NULL)
        return errno;

    library->path = strdup(path);
    if (library->path == NULL) {
        library_close(&library);
        return ENOMEM;
    }

    *library_r = library;
    return 0;
}

void library_close(struct library **library_r) {
    struct library *library;

    assert(library_r != NULL);
    assert(*library_r != NULL);

    library = *library_r;
    *library_r = NULL;

    assert(library->ref == 0);

    if (library->path != NULL)
        free(library->path);

    if (library->plans != NULL) {
        unsigned i;

        for (i = 0; i < library->num_plans; ++i) {
            struct plan_entry *entry = &library->plans[i];
            if (entry->name != NULL)
                free(entry->name);
            if (entry->plan != NULL)
                plan_free(&entry->plan);
        }

        free(library->plans);
    }

    if (library->names)
        free(library->names);

    free(library);
}

static int plan_is_disabled(const struct plan_entry *entry, time_t now) {
    return entry->disabled_until > 0 && now < entry->disabled_until;
}

static int update_plan_names(struct library *library) {
    const time_t now = time(NULL);
    struct strarray plan_names;
    unsigned i;
    const struct plan_entry *entry;

    if (library->names != NULL && now < library->next_names_update)
        return 0;

    library->next_names_update = now + 60;

    /* collect new list */

    strarray_init(&plan_names);

    for (i = 0; i < library->num_plans; ++i) {
        entry = &library->plans[i];

        if (!plan_is_disabled(entry, now))
            strarray_append(&plan_names, entry->name);
    }

    if (library->names != NULL)
        free(library->names);
    library->names = pg_encode_array(&plan_names);

    strarray_free(&plan_names);

    return 0;
}

const char *library_plan_names(struct library *library) {
    update_plan_names(library);

    return library->names == NULL
        ? "{}"
        : library->names;
}
