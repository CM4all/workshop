// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "io/Logger.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <chrono>
#include <memory>
#include <string>
#include <map>

#include <sys/stat.h>

struct Plan;

/**
 * Manage the list of plans (= library) and load their configuration
 * files.
 */
class Library {
	struct PlanEntry {
		std::shared_ptr<Plan> plan;
		bool deinstalled = false;
		struct statx_timestamp mtime{};
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

	const std::string path;
	const UniqueFileDescriptor directory_fd;

	std::map<std::string, PlanEntry, std::less<>> plans;

	std::chrono::steady_clock::time_point next_plans_check =
		std::chrono::steady_clock::time_point::min();

	struct statx_timestamp mtime{};

public:
	explicit Library(const char *path);

	Library(const Library &other) = delete;

	const FileDescriptor GetDirectory() const noexcept {
		return directory_fd;
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
