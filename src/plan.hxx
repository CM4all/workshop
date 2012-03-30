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
struct library;

/** a plan describes how to perform a specific job */
struct plan {
    struct library *library;
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
};

int library_open(const char *path, struct library **library_r);

void library_close(struct library **library_r);

int library_update(struct library *library);

const char *library_plan_names(struct library *library);

int library_get(struct library *library, const char *name,
                struct plan **plan_r);

void plan_put(struct plan **plan_r);

#endif
