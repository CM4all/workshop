// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "control/Handler.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "event/DeferEvent.hxx"
#include "spawn/Registry.hxx"
#include "lib/curl/Init.hxx"
#include "lib/curl/Global.hxx"
#include "io/Logger.hxx"
#include "config.h"

#ifdef HAVE_LIBSYSTEMD
#include "event/systemd/Watchdog.hxx"
#endif

#include <forward_list>

struct Config;
class UniqueSocketDescriptor;
class SpawnServerClient;
class CurlGlobal;
class MultiLibrary;
class ControlServer;
class WorkshopPartition;
class CronPartition;

class Instance final : ControlHandler {
	const RootLogger logger;

	EventLoop event_loop;

#ifdef HAVE_LIBSYSTEMD
	Systemd::Watchdog systemd_watchdog{event_loop};
#endif

	bool should_exit = false;

	ShutdownListener shutdown_listener;
	SignalEvent sighup_event;
	DeferEvent defer_idle_check;

	ChildProcessRegistry child_process_registry;

	std::unique_ptr<SpawnServerClient> spawn_service;

	ScopeCurlInit curl_init;
	CurlGlobal curl;

	std::unique_ptr<MultiLibrary> library;

	std::forward_list<WorkshopPartition> partitions;

	std::forward_list<CronPartition> cron_partitions;

	std::forward_list<ControlServer> control_servers;

public:
	Instance(const Config &config,
		 UniqueSocketDescriptor spawner_socket,
		 std::unique_ptr<MultiLibrary> library);

	~Instance() noexcept;

	EventLoop &GetEventLoop() noexcept {
		return event_loop;
	}

	void Start();

	void Run() noexcept {
		event_loop.Run();
	}

	void UpdateFilter(bool library_modified) noexcept;
	void UpdateLibraryAndFilter(bool force) noexcept;

private:
	void OnExit() noexcept;
	void OnReload(int) noexcept;

	void OnPartitionIdle() noexcept;
	void RemoveIdlePartitions() noexcept;

	/* virtual methods from ControlHandler */
	void OnControlPacket(WorkshopControlCommand command,
			     std::span<const std::byte> payload) override;
	void OnControlError(std::exception_ptr ep) noexcept override;
};
