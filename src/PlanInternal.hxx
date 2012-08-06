/*
 * Internal header for the plan library.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_PLAN_INTERNAL_HXX
#define WORKSHOP_PLAN_INTERNAL_HXX

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
