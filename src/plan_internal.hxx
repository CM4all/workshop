/*
 * Internal header for the plan library.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef __PLAN_INTERNAL_H
#define __PLAN_INTERNAL_H

#include "plan.hxx"

#include <string>
#include <map>

#include <sys/types.h>

struct Plan;

void
plan_free(Plan **plan_r);

struct PlanEntry {
    std::string name;

    Plan *plan;
    bool deinstalled;
    time_t mtime, disabled_until;
    unsigned generation;

    PlanEntry(const char *_name)
        :name(_name), plan(NULL),
         deinstalled(false),
         mtime(0), disabled_until(0),
         generation(0) {}

    PlanEntry(PlanEntry &&other)
        :name(std::move(other.name)), plan(other.plan),
         deinstalled(other.deinstalled),
         mtime(other.mtime), disabled_until(other.disabled_until),
         generation(other.generation) {
        other.plan = NULL;
    }

    PlanEntry(const PlanEntry &other) = delete;

    ~PlanEntry() {
        if (plan != NULL)
            plan_free(&plan);
    }

    bool IsDisabled(time_t now) const {
        return disabled_until > 0 && now < disabled_until;
    }
};

class Library {
public:
    std::string path;

    std::map<std::string, PlanEntry> plans;

    time_t next_plans_check;
    unsigned generation;

    std::string names;

    time_t next_names_update;

    unsigned ref;
    time_t mtime;

    Library(const char *_path)
        :path(_path),
         next_plans_check(0), generation(0),
         next_names_update(0), ref(0), mtime(0) {}

    Library(const Library &other) = delete;
};

/* plan-loader.c */

int
plan_load(const char *path, Plan **plan_r);

/* plan-update.c */

int
library_update_plan(Library &library, PlanEntry &entry);

#endif
