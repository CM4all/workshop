/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_LIBRARY_HXX
#define WORKSHOP_LIBRARY_HXX

#include "Plan.hxx"

#include <inline/compiler.h>

#include <chrono>
#include <memory>
#include <string>
#include <map>

#include <sys/types.h>

struct Plan;

struct PlanEntry {
    std::shared_ptr<Plan> plan;
    bool deinstalled = false;
    time_t mtime = 0;
    std::chrono::steady_clock::time_point disabled_until =
        std::chrono::steady_clock::time_point::min();
    unsigned generation = 0;

    PlanEntry(const char *) {}

    PlanEntry(PlanEntry &&other) = default;

    bool IsDisabled(std::chrono::steady_clock::time_point now) const {
        return now < disabled_until;
    }

    void Disable(std::chrono::steady_clock::time_point now,
                 std::chrono::steady_clock::duration duration) {
        disabled_until = now + duration;
    }
};

class Library {
public:
    const std::string path;

    std::map<std::string, PlanEntry> plans;

    std::chrono::steady_clock::time_point next_plans_check =
        std::chrono::steady_clock::time_point::min();
    unsigned generation = 0;

    std::string names;

    std::chrono::steady_clock::time_point next_names_update =
        std::chrono::steady_clock::time_point::min();

    time_t mtime = 0;

    Library(const char *_path)
        :path(_path) {}

    Library(const Library &other) = delete;

    bool Update();

    gcc_pure
    const char *GetPlanNames() {
        UpdatePlanNames();
        return names.c_str();
    }

    std::shared_ptr<Plan> Get(const char *name);

private:
    PlanEntry &MakePlanEntry(const char *name) {
        return plans.emplace(name, name)
            .first->second;
    }

    int UpdatePlan(const char *name, PlanEntry &entry);
    int UpdatePlans();

    void UpdatePlanNames();
};

#endif
