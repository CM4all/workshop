// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/net/control/Handler.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "event/DeferEvent.hxx"
#include "spawn/Registry.hxx"
#include "lib/curl/Init.hxx"
#include "io/Logger.hxx"
#include "io/StateDirectories.hxx"
#include "config.h"

#ifdef HAVE_LIBSYSTEMD
#include "event/systemd/Watchdog.hxx"
#endif

#ifdef HAVE_AVAHI
#include "lib/avahi/ErrorHandler.hxx"
#endif

#include <forward_list>

struct Config;
class UniqueSocketDescriptor;
class SpawnServerClient;
class MultiLibrary;
namespace Avahi { class Client; class Publisher; }
namespace BengControl { class Server; }
class WorkshopPartition;
class CronPartition;

class Instance final
	: BengControl::Handler
#ifdef HAVE_AVAHI
	, Avahi::ErrorHandler
#endif
{
	const RootLogger logger;

	EventLoop event_loop;

#ifdef HAVE_LIBSYSTEMD
	Systemd::Watchdog systemd_watchdog{event_loop};
#endif

	ShutdownListener shutdown_listener;
	SignalEvent sighup_event;
	DeferEvent defer_idle_check;

	ChildProcessRegistry child_process_registry;

	std::unique_ptr<SpawnServerClient> spawn_service;

	ScopeCurlInit curl_init;

	const StateDirectories state_directories;

#ifdef HAVE_AVAHI
	std::unique_ptr<Avahi::Client> avahi_client;
	std::unique_ptr<Avahi::Publisher> avahi_publisher;
#endif

	std::unique_ptr<MultiLibrary> library;

	std::forward_list<WorkshopPartition> partitions;

	std::forward_list<CronPartition> cron_partitions;

	std::forward_list<BengControl::Server> control_servers;

	bool should_exit = false;

public:
	Instance(const Config &config,
		 UniqueSocketDescriptor spawner_socket,
		 bool cgroups,
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
	void ReloadState() noexcept;

	void OnExit() noexcept;
	void OnReload(int) noexcept;

	void OnPartitionIdle() noexcept;
	void RemoveIdlePartitions() noexcept;

	/* virtual methods from BengControl::Handler */
	void OnControlPacket(BengControl::Command command,
			     std::span<const std::byte> payload,
			     std::span<UniqueFileDescriptor> fds,
			     SocketAddress address, int uid) override;
	void OnControlError(std::exception_ptr &&error) noexcept override;

#ifdef HAVE_AVAHI
	/* virtual methods from Avahi::ErrorHandler */
	bool OnAvahiError(std::exception_ptr error) noexcept override;
#endif // HAVE_AVAHI
};
