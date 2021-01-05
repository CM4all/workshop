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

#ifndef CRON_WORKPLACE_HXX
#define CRON_WORKPLACE_HXX

#include "Operator.hxx"
#include "net/SocketDescriptor.hxx"
#include "util/Compiler.h"

#include <boost/intrusive/list.hpp>

#include <assert.h>

struct CronJob;
class SpawnService;
class EmailService;
class CurlGlobal;
class CronQueue;
class ExitListener;

class CronWorkplace {
	SpawnService &spawn_service;
	EmailService *const email_service;
	const SocketDescriptor pond_socket;

	CurlGlobal &curl;
	ExitListener &exit_listener;

	typedef boost::intrusive::list<CronOperator,
				       boost::intrusive::constant_time_size<true>> OperatorList;

	OperatorList operators;

	const unsigned max_operators;

public:
	CronWorkplace(SpawnService &_spawn_service,
		      EmailService *_email_service,
		      SocketDescriptor _pond_socket,
		      CurlGlobal &_curl,
		      ExitListener &_exit_listener,
		      unsigned _max_operators)
		:spawn_service(_spawn_service),
		 email_service(_email_service),
		 pond_socket(_pond_socket),
		 curl(_curl),
		 exit_listener(_exit_listener),
		 max_operators(_max_operators) {
		assert(max_operators > 0);
	}

	CronWorkplace(const CronWorkplace &other) = delete;

	~CronWorkplace() {
		assert(operators.empty());
	}

	SpawnService &GetSpawnService() {
		return spawn_service;
	}

	EmailService *GetEmailService() {
		return email_service;
	}

	SocketDescriptor GetPondSocket() const noexcept {
		return pond_socket;
	}

	bool IsEmpty() const {
		return operators.empty();
	}

	bool IsFull() const {
		return operators.size() == max_operators;
	}

	/**
	 * Throws std::runtime_error on error.
	 */
	void Start(CronQueue &queue, const char *translation_socket,
		   const char *partition_name, const char *listener_tag,
		   CronJob &&job);

	void OnExit(CronOperator *o);

	void CancelAll() {
		while (!operators.empty())
			operators.front().Cancel();
	}
};

#endif
