// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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

	using OperatorList = IntrusiveList<
		CronOperator,
		IntrusiveListBaseHookTraits<CronOperator>,
		IntrusiveListOptions{.constant_time_size = true}>;

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
