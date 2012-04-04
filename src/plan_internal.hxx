/*
 * Internal header for the plan library.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef __PLAN_INTERNAL_H
#define __PLAN_INTERNAL_H

struct Plan;
struct PlanEntry;
class Library;

/* plan-loader.c */

int
plan_load(const char *path, Plan **plan_r);

/* plan-update.c */

int
library_update_plan(Library &library, PlanEntry &entry);

#endif
