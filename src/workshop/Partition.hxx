// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Queue.hxx"
#include "Workplace.hxx"
#include "spawn/ExitListener.hxx"
#include "event/FineTimerEvent.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "io/Logger.hxx"
#include "time/ExpiryMap.hxx"
#include "util/BindMethod.hxx"

struct Config;
struct WorkshopPartitionConfig;
class Instance;
class MultiLibrary;

class WorkshopPartition final : WorkshopQueueHandler, ExitListener {
	const std::string_view name;

	const Logger logger;

	Instance &instance;
	MultiLibrary &library;

	ExpiryMap<std::string> rate_limited_plans;

	/**
	 * This timer is enabled when #rate_limited_plans is not
	 * empty.  It updates the filter periodically to ensure that
	 * jobs will be picked up again after the rate limit of a plan
	 * expires.
	 */
	FineTimerEvent rate_limit_timer;

	/**
	 * This timer reaps finished jobs.
	 */
	CoarseTimerEvent reap_timer;

	WorkshopQueue queue;
	WorkshopWorkplace workplace;

	BoundMethod<void() noexcept> idle_callback;

	const size_t max_log;

public:
	WorkshopPartition(Instance &instance,
			  MultiLibrary &_library,
			  SpawnService &_spawn_service,
			  const Config &root_config,
			  const WorkshopPartitionConfig &config,
			  BoundMethod<void() noexcept> _idle_callback) noexcept;

	[[nodiscard]]
	auto &GetEventLoop() const noexcept {
		return queue.GetEventLoop();
	}

	[[nodiscard]]
	std::string_view GetName() const noexcept {
		return name;
	}

	[[nodiscard]]
	bool IsIdle() const noexcept {
		return workplace.IsEmpty();
	}

	void Start() noexcept {
		queue.Connect();
	}

	void BeginShutdown() noexcept {
		queue.DisableAdmin();
	}

	void SetStateEnabled(bool _enabled) noexcept {
		queue.SetStateEnabled(_enabled);
	}

	void DisableQueue() noexcept {
		queue.DisableAdmin();
	}

	void EnableQueue() noexcept {
		queue.EnableAdmin();
	}

	void CancelJob(std::string_view id) noexcept {
		workplace.CancelJob(id);
	}

	void TerminateChildren(std::string_view child_tag) noexcept {
		workplace.CancelTag(child_tag);
	}

	void UpdateFilter(bool library_modified=false) noexcept;
	void UpdateLibraryAndFilter(bool force) noexcept;

private:
	void OnRateLimitTimer() noexcept;

	[[nodiscard]]
	std::chrono::seconds CheckRateLimit(const char *plan_name,
					    const Plan &plan) noexcept;

	void OnReapTimer() noexcept;
	void ScheduleReapFinished() noexcept;

	/* virtual methods from WorkshopQueueHandler */
	std::shared_ptr<Plan> GetWorkshopPlan(const char *plan_name) noexcept override;
	bool CheckWorkshopJob(const WorkshopJob &job,
			      const Plan &plan) noexcept override;
	void StartWorkshopJob(WorkshopJob &&job,
			      std::shared_ptr<Plan> plan) noexcept override;

	/* virtual methods from ExitListener */
	void OnChildProcessExit(int status) noexcept override;
};
