/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_LIBRARY_HXX
#define WORKSHOP_LIBRARY_HXX

#include <inline/compiler.h>

#include <boost/filesystem.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <map>

#include <sys/types.h>

struct Plan;

class Library {
    struct PlanEntry {
        std::shared_ptr<Plan> plan;
        bool deinstalled = false;
        time_t mtime = 0;
        std::chrono::steady_clock::time_point disabled_until =
            std::chrono::steady_clock::time_point::min();

        void Clear() {
            plan.reset();
            mtime = 0;
        }

        bool IsDisabled(std::chrono::steady_clock::time_point now) const {
            return now < disabled_until;
        }

        bool IsAvailable(std::chrono::steady_clock::time_point now) const {
            return !deinstalled && !IsDisabled(now);
        }

        void Disable(std::chrono::steady_clock::time_point now,
                     std::chrono::steady_clock::duration duration) {
            disabled_until = now + duration;
        }

        void Enable() {
            disabled_until = std::chrono::steady_clock::time_point::min();
        }
    };

    const boost::filesystem::path path;

    std::map<std::string, PlanEntry> plans;

    std::chrono::steady_clock::time_point next_plans_check =
        std::chrono::steady_clock::time_point::min();

    std::string names;

    time_t mtime = 0;

public:
    explicit Library(boost::filesystem::path &&_path)
        :path(std::move(_path)) {}

    Library(const Library &other) = delete;

    const boost::filesystem::path &GetPath() const {
        return path;
    }

    void ScheduleNamesUpdate() {
        names.clear();
    }

    /**
     * @param force if true, then update the list even if the plan
     * directory time stamp hasn't changed and the #next_plans_check
     * time stamp hasn't been reached yet
     *
     * @return true if the library was modified (at least one plan has
     * been added, modified or deleted)
     */
    bool Update(bool force);

    gcc_pure
    const char *GetPlanNames() {
        UpdatePlanNames();
        return names.c_str();
    }

    std::shared_ptr<Plan> Get(const char *name);

private:
    PlanEntry &MakePlanEntry(const char *name) {
        return plans.emplace(std::piecewise_construct,
                             std::forward_as_tuple(name),
                             std::forward_as_tuple())
            .first->second;
    }

    void DisablePlan(PlanEntry &entry,
                     std::chrono::steady_clock::time_point now,
                     std::chrono::steady_clock::duration duration);

    /**
     * @return true if the plan file was modified
     */
    bool CheckPlanModified(const char *name, PlanEntry &entry,
                           std::chrono::steady_clock::time_point now);

    /**
     * Check whether the plan is available.
     */
    bool ValidatePlan(PlanEntry &entry,
                      std::chrono::steady_clock::time_point now);

    bool LoadPlan(const char *name, PlanEntry &entry,
                  std::chrono::steady_clock::time_point now);

    /**
     * @return whether the plan was modified
     */
    bool UpdatePlan(const char *name, PlanEntry &entry,
                    std::chrono::steady_clock::time_point now);

    /**
     * @return whether the plan was modified
     */
    bool UpdatePlans();

    void UpdatePlanNames();
};

#endif
