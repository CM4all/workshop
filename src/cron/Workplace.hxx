// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/SocketDescriptor.hxx"
#include "util/IntrusiveList.hxx"

struct CronJob;
class SpawnService;
class EmailService;
class CronQueue;
class ExitListener;

class CronWorkplace {
	SpawnService &spawn_service;
	EmailService &email_service;
	const std::string_view default_email_sender;
	const SocketDescriptor pond_socket;

	ExitListener &exit_listener;

	class Running;
	IntrusiveList<Running,
		IntrusiveListBaseHookTraits<Running>,
		IntrusiveListOptions{.constant_time_size = true}> running;

	const std::size_t max_operators;

public:
	CronWorkplace(SpawnService &_spawn_service,
		      EmailService &_email_service,
		      std::string_view _default_email_sender,
		      SocketDescriptor _pond_socket,
		      ExitListener &_exit_listener,
		      std::size_t _max_operators);

	CronWorkplace(const CronWorkplace &other) = delete;
	CronWorkplace &operator=(const CronWorkplace &other) = delete;

	~CronWorkplace() noexcept;

	SpawnService &GetSpawnService() {
		return spawn_service;
	}

	SocketDescriptor GetPondSocket() const noexcept {
		return pond_socket;
	}

	bool IsEmpty() const {
		return running.empty();
	}

	bool IsFull() const {
		return running.size() >= max_operators;
	}

	/**
	 * Throws std::runtime_error on error.
	 */
	void Start(CronQueue &queue, SocketAddress translation_socket,
		   std::string_view partition_name, const char *listener_tag,
		   CronJob &&job);

	void CancelAll() noexcept;

	void CancelTag(std::string_view tag) noexcept;

private:
	void OnCompletion(Running &r) noexcept;
};
