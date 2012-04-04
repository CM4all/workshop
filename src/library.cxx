/*
 * Manage the list of plans (= library) and load their configuration
 * files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "library.hxx"
#include "plan.hxx"
#include "plan_internal.hxx"
#include "pg_array.hxx"

#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>

int
library_open(const char *path, Library **library_r)
{
    int ret;
    struct stat st;

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

    Library *library = new Library(path);
    *library_r = library;
    return 0;
}

void
library_close(Library **library_r)
{
    assert(library_r != NULL);
    assert(*library_r != NULL);

    Library *library = *library_r;
    *library_r = NULL;

    delete library;
}

static void
update_plan_names(Library &library)
{
    const time_t now = time(NULL);

    if (!library.names.empty() && now < library.next_names_update)
        return;

    library.next_names_update = now + 60;

    /* collect new list */

    std::list<std::string> plan_names;

    for (const auto &i : library.plans) {
        const std::string &name = i.first;
        const PlanEntry &entry = i.second;

        if (!entry.IsDisabled(now))
            plan_names.push_back(name);
    }

    library.names = pg_encode_array(plan_names);
}

const char *
library_plan_names(Library *library)
{
    update_plan_names(*library);

    return library->names.c_str();
}
