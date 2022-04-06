/*
 * Copyright 2006-2021 CM4all GmbH
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

#pragma once

#include "io/Logger.hxx"

#include <boost/intrusive/list.hpp>

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

	using OperatorList =
		boost::intrusive::list<WorkshopOperator,
				       boost::intrusive::base_hook<boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>>,
				       boost::intrusive::constant_time_size<true>>;

	OperatorList operators;

	const unsigned max_operators;
	const bool enable_journal;

public:
	WorkshopWorkplace(SpawnService &_spawn_service,
			  ExitListener &_exit_listener,
			  const Logger &parent_logger,
			  const char *_node_name,
			  unsigned _max_operators,
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
