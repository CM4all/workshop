// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Queue.hxx"
#include "Workplace.hxx"
#include "spawn/ExitListener.hxx"
#include "event/Chrono.hxx"
#include "net/SocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/Logger.hxx"
#include "util/BindMethod.hxx"
#include "EmailService.hxx"

#include <string_view>

struct Config;
struct CronPartitionConfig;
class EventLoop;
class SpawnService;
class EmailService;

class CronPartition final : ExitListener {
	const std::string_view name;
	const char *const tag;

	const SocketAddress translation_socket;

	const Logger logger;

	EmailService email_service;

	const UniqueSocketDescriptor pond_socket;

	CronQueue queue;

	CronWorkplace workplace;

	BoundMethod<void() noexcept> idle_callback;

	const Event::Duration default_timeout;

public:
	CronPartition(EventLoop &event_loop,
		      SpawnService &_spawn_service,
		      const Config &root_config,
		      const CronPartitionConfig &config,
		      BoundMethod<void() noexcept> _idle_callback);

	~CronPartition() noexcept;

	[[nodiscard]]
	std::string_view GetName() const noexcept {
		return name;
	}

	[[nodiscard]]
	bool IsIdle() const noexcept {
		return workplace.IsEmpty();
	}

	void BeginShutdown() noexcept;

	void SetStateEnabled(bool _enabled) noexcept {
		queue.SetStateEnabled(_enabled);
	}

	void DisableQueue() noexcept {
		queue.DisableAdmin();
	}

	void EnableQueue() noexcept {
		queue.EnableAdmin();
	}

	void TerminateChildren(std::string_view child_tag) noexcept {
		workplace.CancelTag(child_tag);
	}

private:
	void OnJob(CronJob &&job) noexcept;

	/* virtual methods from ExitListener */
	void OnChildProcessExit(int status) noexcept override;
};
