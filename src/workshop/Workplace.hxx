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

#ifndef WORKSHOP_WORKPLACE_HXX
#define WORKSHOP_WORKPLACE_HXX

#include "Operator.hxx"
#include "io/Logger.hxx"
#include "util/Compiler.h"

#include <boost/intrusive/list.hpp>

#include <memory>
#include <string>

#include <assert.h>

struct Plan;
struct WorkshopJob;
class SpawnService;
class ExitListener;

class WorkshopWorkplace {
	SpawnService &spawn_service;
	ExitListener &exit_listener;

	const ChildLogger logger;

	const std::string node_name;

	typedef boost::intrusive::list<WorkshopOperator,
				       boost::intrusive::constant_time_size<true>> OperatorList;

	OperatorList operators;

	const unsigned max_operators;
	const bool enable_journal;

public:
	WorkshopWorkplace(SpawnService &_spawn_service,
			  ExitListener &_exit_listener,
			  const Logger &parent_logger,
			  const char *_node_name,
			  unsigned _max_operators,
			  bool _enable_journal)
		:spawn_service(_spawn_service), exit_listener(_exit_listener),
		 logger(parent_logger, "workplace"),
		 node_name(_node_name),
		 max_operators(_max_operators),
		 enable_journal(_enable_journal)
	{
		assert(max_operators > 0);
	}

	WorkshopWorkplace(const WorkshopWorkplace &other) = delete;

	~WorkshopWorkplace() {
		assert(operators.empty());
	}

	gcc_pure
	const char *GetNodeName() const {
		return node_name.c_str();
	}

	bool IsEmpty() const {
		return operators.empty();
	}

	bool IsFull() const {
		return operators.size() == max_operators;
	}

	gcc_pure
	std::string GetRunningPlanNames() const;

	/**
	 * Returns the plan names which have reached their concurrency
	 * limit.
	 */
	gcc_pure
	std::string GetFullPlanNames() const;

	/**
	 * Throws std::runtime_error on error.
	 */
	void Start(EventLoop &event_loop, const WorkshopJob &job,
		   std::shared_ptr<Plan> plan,
		   size_t max_log);

	void OnExit(WorkshopOperator *o);
	void OnTimeout(WorkshopOperator *o, int pid);
};

#endif
