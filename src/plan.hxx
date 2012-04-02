/*
 * Internal declarations of cm4all-workshop.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_PLAN_H
#define WORKSHOP_PLAN_H

#include "strarray.h"

#include <sys/types.h>

/** a library is a container for plan objects */
class Library;

/** a plan describes how to perform a specific job */
struct plan {
    Library *library;
    struct strarray argv;
    char *timeout, *chroot;
    uid_t uid;
    gid_t gid;

    /** number of supplementary groups */
    int num_groups;
    /** supplementary group ids */
    gid_t *groups;

    int priority;

    /** maximum concurrency for this plan */
    unsigned concurrency;

    unsigned ref;

    plan(const plan &other) = delete;
};

int
library_open(const char *path, Library **library_r);

void
library_close(Library **library_r);

int
library_update(Library *library);

const char *
library_plan_names(Library *library);

int
library_get(Library *library, const char *name,
            struct plan **plan_r);

void plan_put(struct plan **plan_r);

#endif
