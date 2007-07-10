/*
 * Internal header for the plan library.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef __PLAN_INTERNAL_H
#define __PLAN_INTERNAL_H

#include <sys/types.h>

struct plan;

struct plan_entry {
    char *name;
    struct plan *plan;
    int deinstalled;
    time_t mtime, disabled_until;
    unsigned generation;
};

struct library {
    char *path;

    struct plan_entry *plans;
    unsigned num_plans, max_plans;
    time_t next_plans_check;
    unsigned generation;

    char *names;
    time_t next_names_update;

    unsigned ref;
    time_t mtime;
};

/* plan-loader.c */

void plan_free(struct plan **plan_r);

int plan_load(const char *path, struct plan **plan_r);

/* plan-update.c */

int library_update_plan(struct library *library, struct plan_entry *entry);

#endif
