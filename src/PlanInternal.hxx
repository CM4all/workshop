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

Plan *
plan_load(const char *path);

#endif
