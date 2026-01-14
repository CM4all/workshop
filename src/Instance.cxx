// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Instance.hxx"
#include "Config.hxx"
#include "event/net/control/Server.hxx"
#include "workshop/MultiLibrary.hxx"
#include "workshop/Partition.hxx"
#include "cron/Partition.hxx"
#include "spawn/Client.hxx"
#include "util/SpanCast.hxx"
#include "util/StringSplit.hxx"

#ifdef HAVE_AVAHI
#include "lib/avahi/Client.hxx"
#include "lib/avahi/Publisher.hxx"
#endif

#include <fmt/core.h>

#include <signal.h>

using std::string_view_literals::operator""sv;

Instance::Instance(const Config &config,
		   UniqueSocketDescriptor spawner_socket,
		   bool cgroups,
		   std::unique_ptr<MultiLibrary> _library)
	:shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
	 sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
	 defer_idle_check(event_loop, BIND_THIS_METHOD(RemoveIdlePartitions)),
	 spawn_service(new SpawnServerClient(event_loop,
					     config.spawn,
					     std::move(spawner_socket),
					     cgroups,
					     /* disable "verify", we
						do it via SpawnHook */
					     false)),
	 library(std::move(_library))
{
	shutdown_listener.Enable();
	sighup_event.Enable();

#ifdef HAVE_AVAHI
	Avahi::ErrorHandler &avahi_error_handler = *this;
	if (config.UsesZeroconf()) {
		avahi_client.reset(new Avahi::Client(event_loop, *this));
		avahi_publisher.reset(new Avahi::Publisher(*avahi_client, config.node_name.c_str(),
							   avahi_error_handler));
	}
#endif // HAVE_AVAHI

	for (const auto &i : config.partitions)
		partitions.emplace_front(*this, *library, *spawn_service,
#ifdef HAVE_AVAHI
					 avahi_client.get(),
					 avahi_publisher.get(),
					 avahi_error_handler,
#endif // HAVE_AVAHI
					 config, i,
					 BIND_THIS_METHOD(OnPartitionIdle));

	for (const auto &i : config.cron_partitions)
		cron_partitions.emplace_front(event_loop, *spawn_service,
#ifdef HAVE_AVAHI
					      avahi_client.get(),
					      avahi_publisher.get(),
					      avahi_error_handler,
#endif // HAVE_AVAHI
					      config, i,
					      BIND_THIS_METHOD(OnPartitionIdle));

	BengControl::Handler &control_handler = *this;
	for (const auto &i : config.control_listen)
		control_servers.emplace_front(event_loop, i.Create(SOCK_DGRAM),
					      control_handler);
}

Instance::~Instance() noexcept = default;

void
Instance::Start()
{
	ReloadState();

	for (auto &i : partitions)
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

	if (!partitions.empty() || !cron_partitions.empty())
		logger(1, "waiting for jobs to finish");

	control_servers.clear();

#ifdef HAVE_AVAHI
	avahi_publisher.reset();
	avahi_client.reset();
#endif // HAVE_AVAHI
}

void
Instance::ReloadState() noexcept
{
	for (auto &i : partitions) {
		const auto path = fmt::format("workshop/workshop/{}/enabled"sv,
					      i.GetName());
		i.SetStateEnabled(state_directories.GetBool(path.c_str(), true));
	}

	for (auto &i : cron_partitions) {
		const std::string_view name = i.GetName();
		if (name.empty())
			/* anonymous partitions cannot have state */
			continue;

		const auto path = fmt::format("workshop/cron/{}/enabled"sv, name);
		i.SetStateEnabled(state_directories.GetBool(path.c_str(), true));
	}
}

void
Instance::OnReload(int) noexcept
{
	logger(4, "reloading");

	if (library)
		UpdateLibraryAndFilter(true);

	ReloadState();
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
Instance::OnControlPacket(BengControl::Command command,
			  std::span<const std::byte> payload,
			  [[maybe_unused]] std::span<UniqueFileDescriptor> fds,
			  [[maybe_unused]] SocketAddress address,
			  int uid)
{
	using namespace BengControl;

	/* only local clients are allowed to use most commands */
	const bool is_privileged = uid >= 0;

	switch (command) {
	case Command::NOP:
		break;

	case Command::TCACHE_INVALIDATE:
	case Command::DUMP_POOLS:
	case Command::ENABLE_NODE:
	case Command::FADE_NODE:
	case Command::NODE_STATUS:
	case Command::STATS:
	case Command::FADE_CHILDREN:
	case Command::DISABLE_ZEROCONF:
	case Command::ENABLE_ZEROCONF:
	case Command::FLUSH_NFS_CACHE:
	case Command::FLUSH_FILTER_CACHE:
	case Command::STOPWATCH_PIPE:
	case Command::DISCARD_SESSION:
	case Command::FLUSH_HTTP_CACHE:
	case Command::DISCONNECT_DATABASE:
	case Command::DISABLE_URING:
	case Command::RESET_LIMITER:
	case Command::REJECT_CLIENT:
	case Command::TARPIT_CLIENT:
		// not applicable
		break;

	case Command::VERBOSE:
		if (is_privileged) {
			const auto *log_level = (const uint8_t *)payload.data();
			if (payload.size() != sizeof(*log_level))
				throw std::runtime_error("Malformed VERBOSE packet");

			SetLogLevel(*log_level);
		}
		break;

	case Command::TERMINATE_CHILDREN:
		if (const auto tag = ToStringView(payload); !tag.empty()) {
			for (auto &i : partitions)
				i.TerminateChildren(tag);
			for (auto &i : cron_partitions)
				i.TerminateChildren(tag);
		}

		break;

	case Command::DISABLE_QUEUE:
		if (!is_privileged)
			break;

		if (payload.empty()) {
			logger(2, "Disabling all queues");
			for (auto &i : partitions)
				i.DisableQueue();
			for (auto &i : cron_partitions)
				i.DisableQueue();
		} else {
			const std::string_view name = ToStringView(payload);
			for (auto &i : partitions) {
				if (name == i.GetName()) {
					logger.Fmt(2, "Disabling queue {:?}", name);
					i.DisableQueue();
				}
			}
			for (auto &i : cron_partitions) {
				if (name == i.GetName()) {
					logger.Fmt(2, "Disabling queue {:?}", name);
					i.DisableQueue();
				}
			}
		}

		break;

	case Command::ENABLE_QUEUE:
		if (!is_privileged)
			break;

		if (payload.empty()) {
			logger(2, "Enabling all queues");
			for (auto &i : partitions)
				i.EnableQueue();
			for (auto &i : cron_partitions)
				i.EnableQueue();
		} else {
			const std::string_view name = ToStringView(payload);
			for (auto &i : partitions) {
				if (name == i.GetName()) {
					logger.Fmt(2, "Enabling queue {:?}", name);
					i.EnableQueue();
				}
			}
			for (auto &i : cron_partitions) {
				if (name == i.GetName()) {
					logger.Fmt(2, "Enabling queue {:?}", name);
					i.EnableQueue();
				}
			}
		}

		break;

	case Command::RELOAD_STATE:
		if (is_privileged)
			ReloadState();
		break;

	case Command::CANCEL_JOB:
		if (const auto [partition_name, job_id] = Split(ToStringView(payload), '\0');
		    !job_id.empty()) {
			for (auto &i : partitions)
				if (partition_name == i.GetName())
					i.CancelJob(job_id);
		}

		break;
	}
}

void
Instance::OnControlError(std::exception_ptr &&error) noexcept
{
	logger(1, "Control error: ", std::move(error));
}

#ifdef HAVE_AVAHI

bool
Instance::OnAvahiError(std::exception_ptr error) noexcept
{
	logger(1, "Avahi error: ", std::move(error));
	return true;
}

#endif // HAVE_AVAHI
