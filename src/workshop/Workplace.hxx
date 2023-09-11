// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "io/Logger.hxx"
#include "net/SocketAddress.hxx"
#include "util/IntrusiveList.hxx"

#include <memory>
#include <string>

struct Plan;
struct WorkshopJob;
class WorkshopOperator;
class EventLoop;
class SpawnService;
class ExitListener;

class WorkshopWorkplace {
	SpawnService &spawn_service;
	ExitListener &exit_listener;

	const ChildLogger logger;

	const std::string node_name;

	using OperatorList = IntrusiveList<
		WorkshopOperator,
		IntrusiveListBaseHookTraits<WorkshopOperator>,
		IntrusiveListOptions{.constant_time_size = true}>;

	OperatorList operators;

	const SocketAddress translation_socket;
	const char *const listener_tag;

	const std::size_t max_operators;
	const bool enable_journal;

public:
	WorkshopWorkplace(SpawnService &_spawn_service,
			  ExitListener &_exit_listener,
			  const Logger &parent_logger,
			  const char *_node_name,
			  SocketAddress _translation_socket,
			  const char *_listener_tag,
			  std::size_t _max_operators,
			  bool _enable_journal) noexcept;

	WorkshopWorkplace(const WorkshopWorkplace &other) = delete;
	WorkshopWorkplace &operator=(const WorkshopWorkplace &other) = delete;

	~WorkshopWorkplace() noexcept;

	[[gnu::pure]]
	const char *GetNodeName() const noexcept {
		return node_name.c_str();
	}

	bool IsEmpty() const noexcept {
		return operators.empty();
	}

	bool IsFull() const noexcept {
		return operators.size() == max_operators;
	}

	auto &GetSpawnService() const noexcept {
		return spawn_service;
	}

	SocketAddress GetTranslationSocket() const noexcept {
		return translation_socket;
	}

	const char *GetListenerTag() const noexcept {
		return listener_tag;
	}

	[[gnu::pure]]
	std::string GetRunningPlanNames() const noexcept;

	/**
	 * Returns the plan names which have reached their concurrency
	 * limit.
	 */
	[[gnu::pure]]
	std::string GetFullPlanNames() const noexcept;

	/**
	 * Throws std::runtime_error on error.
	 */
	void Start(EventLoop &event_loop, const WorkshopJob &job,
		   std::shared_ptr<Plan> plan,
		   size_t max_log);

	void OnExit(WorkshopOperator *o) noexcept;
	void OnTimeout(WorkshopOperator *o) noexcept;
};
