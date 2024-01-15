// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Config.hxx"
#include "event/net/control/Server.hxx"
#include "workshop/MultiLibrary.hxx"
#include "workshop/Partition.hxx"
#include "cron/Partition.hxx"
#include "spawn/Client.hxx"
#include "util/SpanCast.hxx"

#include <signal.h>

Instance::Instance(const Config &config,
		   UniqueSocketDescriptor spawner_socket,
		   std::unique_ptr<MultiLibrary> _library)
	:shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
	 sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
	 defer_idle_check(event_loop, BIND_THIS_METHOD(RemoveIdlePartitions)),
	 spawn_service(new SpawnServerClient(event_loop,
					     config.spawn,
					     std::move(spawner_socket),
					     /* disable "verify", we
						do it via SpawnHook */
					     false)),
	 curl(event_loop),
	 library(std::move(_library))
{
	shutdown_listener.Enable();
	sighup_event.Enable();

	for (const auto &i : config.partitions)
		partitions.emplace_front(*this, *library, *spawn_service,
					 config, i,
					 BIND_THIS_METHOD(OnPartitionIdle));

	for (const auto &i : config.cron_partitions)
		cron_partitions.emplace_front(event_loop, *spawn_service, curl,
					      config, i,
					      BIND_THIS_METHOD(OnPartitionIdle));

	ControlHandler &control_handler = *this;
	for (const auto &i : config.control_listen)
		control_servers.emplace_front(event_loop, i.Create(SOCK_DGRAM),
					      control_handler);
}

Instance::~Instance() noexcept = default;

void
Instance::Start()
{
	for (auto &i : partitions)
		i.Start();
	for (auto &i : cron_partitions)
		i.Start();

	if (library)
		UpdateLibraryAndFilter(true);
}

void
Instance::UpdateFilter(bool library_modified) noexcept
{
	for (auto &i : partitions)
		i.UpdateFilter(library_modified);
}

void
Instance::UpdateLibraryAndFilter(bool force) noexcept
{
	assert(library);

	const bool library_modified =
		library->Update(event_loop.SteadyNow(), force);
	UpdateFilter(library_modified);
}

void
Instance::OnExit() noexcept
{
	if (should_exit)
		return;

	should_exit = true;

#ifdef HAVE_LIBSYSTEMD
	systemd_watchdog.Disable();
#endif

	shutdown_listener.Disable();
	sighup_event.Disable();

	spawn_service->Shutdown();

	for (auto &i : partitions)
		i.BeginShutdown();

	for (auto &i : cron_partitions)
		i.BeginShutdown();

	RemoveIdlePartitions();

	if (!partitions.empty())
		logger(1, "waiting for operators to finish");

	control_servers.clear();
}

void
Instance::OnReload(int) noexcept
{
	logger(4, "reloading");

	if (library)
		UpdateLibraryAndFilter(true);
}

void
Instance::OnPartitionIdle() noexcept
{
	if (should_exit)
		/* defer the RemoveIdlePartitions() call to avoid deleting the
		   partitions while iterating the partition list in
		   OnExit() */
		defer_idle_check.Schedule();
}

void
Instance::RemoveIdlePartitions() noexcept
{
	partitions.remove_if([](const WorkshopPartition &partition){
			return partition.IsIdle();
		});

	cron_partitions.remove_if([](const CronPartition &partition){
			return partition.IsIdle();
		});
}

void
Instance::OnControlPacket([[maybe_unused]] ControlServer &control_server,
			  BengProxy::ControlCommand command,
			  std::span<const std::byte> payload,
			  [[maybe_unused]] std::span<UniqueFileDescriptor> fds,
			  [[maybe_unused]] SocketAddress address,
			  [[maybe_unused]] int uid)
{
	/* only local clients are allowed to use most commands */
	const bool is_privileged = uid >= 0;

	switch (command) {
	case BengProxy::ControlCommand::NOP:
		break;

	case BengProxy::ControlCommand::TCACHE_INVALIDATE:
	case BengProxy::ControlCommand::DUMP_POOLS:
	case BengProxy::ControlCommand::ENABLE_NODE:
	case BengProxy::ControlCommand::FADE_NODE:
	case BengProxy::ControlCommand::NODE_STATUS:
	case BengProxy::ControlCommand::STATS:
	case BengProxy::ControlCommand::FADE_CHILDREN:
	case BengProxy::ControlCommand::DISABLE_ZEROCONF:
	case BengProxy::ControlCommand::ENABLE_ZEROCONF:
	case BengProxy::ControlCommand::FLUSH_NFS_CACHE:
	case BengProxy::ControlCommand::FLUSH_FILTER_CACHE:
	case BengProxy::ControlCommand::STOPWATCH_PIPE:
	case BengProxy::ControlCommand::DISCARD_SESSION:
	case BengProxy::ControlCommand::FLUSH_HTTP_CACHE:
		// not applicable
		break;

	case BengProxy::ControlCommand::VERBOSE:
		if (is_privileged) {
			const auto *log_level = (const uint8_t *)payload.data();
			if (payload.size() != sizeof(*log_level))
				throw std::runtime_error("Malformed VERBOSE packet");

			SetLogLevel(*log_level);
		}
		break;

	case BengProxy::ControlCommand::TERMINATE_CHILDREN:
		if (const auto tag = ToStringView(payload); !tag.empty())
			for (auto &i : cron_partitions)
				i.TerminateChildren(tag);
		break;

	case BengProxy::ControlCommand::DISABLE_QUEUE:
		if (!is_privileged)
			break;

		logger(2, "Disabling all queues");
		for (auto &i : partitions)
			i.DisableQueue();
		for (auto &i : cron_partitions)
			i.DisableQueue();
		break;

	case BengProxy::ControlCommand::ENABLE_QUEUE:
		if (!is_privileged)
			break;

		logger(2, "Enabling all queues");
		for (auto &i : partitions)
			i.EnableQueue();
		for (auto &i : cron_partitions)
			i.EnableQueue();
		break;
	}
}

void
Instance::OnControlError(std::exception_ptr ep) noexcept
{
	logger(1, "Control error: ", ep);
}
