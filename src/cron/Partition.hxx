// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Queue.hxx"
#include "Workplace.hxx"
#include "spawn/ExitListener.hxx"
#include "net/SocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/Logger.hxx"
#include "util/BindMethod.hxx"

#include <memory>

struct Config;
struct CronPartitionConfig;
class EventLoop;
class SpawnService;
class EmailService;

class CronPartition final : ExitListener {
	const char *const name;
	const char *const tag;

	const SocketAddress translation_socket;

	Logger logger;

	std::unique_ptr<EmailService> email_service;

	const UniqueSocketDescriptor pond_socket;

	CronQueue queue;

	CronWorkplace workplace;

	BoundMethod<void() noexcept> idle_callback;

public:
	CronPartition(EventLoop &event_loop,
		      SpawnService &_spawn_service,
		      CurlGlobal &_curl,
		      const Config &root_config,
		      const CronPartitionConfig &config,
		      BoundMethod<void() noexcept> _idle_callback);

	~CronPartition() noexcept;

	[[gnu::pure]]
	bool IsName(std::string_view _name) const noexcept {
		return name != nullptr && _name == name;
	}

	[[nodiscard]]
	bool IsIdle() const noexcept {
		return workplace.IsEmpty();
	}

	void Start() noexcept {
		queue.Connect();
	}

	void BeginShutdown() noexcept;

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
