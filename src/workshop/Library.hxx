// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef WORKSHOP_LIBRARY_HXX
#define WORKSHOP_LIBRARY_HXX

#include "io/Logger.hxx"

#include <chrono>
#include <filesystem>
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
		std::filesystem::file_time_type mtime{};
		std::chrono::steady_clock::time_point disabled_until =
			std::chrono::steady_clock::time_point::min();

		void Clear() {
			plan.reset();
			mtime = {};
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

	const std::filesystem::path path;

	std::map<std::string, PlanEntry, std::less<>> plans;

	std::chrono::steady_clock::time_point next_plans_check =
		std::chrono::steady_clock::time_point::min();

	std::filesystem::file_time_type mtime{};

public:
	explicit Library(std::filesystem::path &&_path) noexcept
		:logger("library"),
		 path(std::move(_path)) {}

	Library(const Library &other) = delete;

	const std::filesystem::path &GetPath() const noexcept {
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
	bool Update(std::chrono::steady_clock::time_point now, bool force) noexcept;

	/**
	 * Visit all plans that are available.
	 */
	template<typename F>
	void VisitAvailable(std::chrono::steady_clock::time_point now,
			    F &&f) const {
		for (const auto &i : plans) {
			const std::string &name = i.first;
			const PlanEntry &entry = i.second;

			if (entry.IsAvailable(now))
				f(name, *entry.plan);
		}
	}

	std::shared_ptr<Plan> Get(std::chrono::steady_clock::time_point now,
				  const char *name) noexcept;

private:
	void DisablePlan(PlanEntry &entry,
			 std::chrono::steady_clock::time_point now,
			 std::chrono::steady_clock::duration duration) noexcept;

	/**
	 * @return true if the plan file was modified
	 */
	bool CheckPlanModified(const char *name, PlanEntry &entry,
			       std::chrono::steady_clock::time_point now) noexcept;

	/**
	 * Check whether the plan is available.
	 */
	bool ValidatePlan(PlanEntry &entry,
			  std::chrono::steady_clock::time_point now) noexcept;

	bool LoadPlan(const char *name, PlanEntry &entry,
		      std::chrono::steady_clock::time_point now) noexcept;

	/**
	 * @return whether the plan was modified
	 */
	bool UpdatePlan(const char *name, PlanEntry &entry,
			std::chrono::steady_clock::time_point now) noexcept;

	/**
	 * @return whether the plan was modified
	 */
	bool UpdatePlans(std::chrono::steady_clock::time_point now);
};

#endif
