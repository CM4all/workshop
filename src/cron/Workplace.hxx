/*
 * Copyright 2006-2022 CM4all GmbH
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

#include "net/SocketDescriptor.hxx"
#include "util/IntrusiveList.hxx"

struct CronJob;
class CronOperator;
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

	using OperatorList =
		IntrusiveList<CronOperator,
			      IntrusiveListBaseHookTraits<CronOperator>,
			      true>;

	OperatorList operators;

	const std::size_t max_operators;

public:
	CronWorkplace(SpawnService &_spawn_service,
		      EmailService *_email_service,
		      SocketDescriptor _pond_socket,
		      CurlGlobal &_curl,
		      ExitListener &_exit_listener,
		      std::size_t _max_operators);

	CronWorkplace(const CronWorkplace &other) = delete;
	CronWorkplace &operator=(const CronWorkplace &other) = delete;

	~CronWorkplace() noexcept;

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
	void Start(CronQueue &queue, SocketAddress translation_socket,
		   const char *partition_name, const char *listener_tag,
		   CronJob &&job);

	void OnExit(CronOperator *o);

	void CancelAll() noexcept;
};
