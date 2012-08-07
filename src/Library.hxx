/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_LIBRARY_HXX
#define WORKSHOP_LIBRARY_HXX

#include "Plan.hxx"

#include <inline/compiler.h>

#include <string>
#include <map>

#include <sys/types.h>

struct Plan;

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
        delete plan;
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

    static Library *Open(const char *path);

    bool Update();

    gcc_pure
    const char *GetPlanNames() {
        UpdatePlanNames();
        return names.c_str();
    }

    Plan *Get(const char *name);

private:
    PlanEntry &MakePlanEntry(const char *name) {
        return plans.insert(std::make_pair(name, PlanEntry(name)))
            .first->second;
    }

    int UpdatePlan(PlanEntry &entry);
    int UpdatePlans();

    void UpdatePlanNames();
};

#endif
