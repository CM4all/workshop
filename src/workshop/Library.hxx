/*
 * Copyright 2006-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WORKSHOP_LIBRARY_HXX
#define WORKSHOP_LIBRARY_HXX

#include "io/Logger.hxx"

#include <boost/filesystem.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <map>

#include <sys/types.h>

struct Plan;

/**
 * Manage the list of plans (= library) and load their configuration
 * files.
 */
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

    const LLogger logger;

    const boost::filesystem::path path;

    std::map<std::string, PlanEntry> plans;

    std::chrono::steady_clock::time_point next_plans_check =
        std::chrono::steady_clock::time_point::min();

    time_t mtime = 0;

public:
    explicit Library(boost::filesystem::path &&_path)
        :logger("library"),
         path(std::move(_path)) {}

    Library(const Library &other) = delete;

    const boost::filesystem::path &GetPath() const {
        return path;
    }

    /**
     * @param force if true, then update the list even if the plan
     * directory time stamp hasn't changed and the #next_plans_check
     * time stamp hasn't been reached yet
     *
     * @return true if the library was modified (at least one plan has
     * been added, modified or deleted)
     */
    bool Update(std::chrono::steady_clock::time_point now, bool force);

    template<typename F>
    void VisitPlans(std::chrono::steady_clock::time_point now, F &&f) const {
        for (const auto &i : plans) {
            const std::string &name = i.first;
            const PlanEntry &entry = i.second;

            if (entry.IsAvailable(now))
                f(name, *entry.plan);
        }
    }

    std::shared_ptr<Plan> Get(std::chrono::steady_clock::time_point now,
                              const char *name);

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
    bool UpdatePlans(std::chrono::steady_clock::time_point now);
};

#endif
