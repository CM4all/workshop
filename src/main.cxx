// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "debug.h"
#include "Instance.hxx"
#include "Hook.hxx"
#include "CommandLine.hxx"
#include "Config.hxx"
#include "workshop/MultiLibrary.hxx"
#include "spawn/Launch.hxx"
#include "system/KernelVersion.hxx"
#include "system/SetupProcess.hxx"
#include "system/ProcessName.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/PrintException.hxx"
#include "config.h"

#ifdef HAVE_LIBCAP
#include "lib/cap/Glue.hxx"
#include "lib/cap/State.hxx"
#endif

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <stdexcept>

#include <stdlib.h>

#ifndef NDEBUG
bool debug_mode = false;
#endif

static auto
MakeLibrary()
{
	auto library = std::make_unique<MultiLibrary>();
	library->InsertPath("/etc/cm4all/workshop/plans");
	library->InsertPath("/usr/share/cm4all/workshop/plans");
	return library;
}

static void
DropInheritableCapabilities()
{
#ifdef HAVE_LIBCAP
	/* don't inherit any capabilities to spawned processes */
	auto state = CapabilityState::Current();
	state.ClearFlag(CAP_INHERITABLE);
	state.Install();
#endif
}

static void
Run(const Config &config)
{
	SetupProcess();

	std::unique_ptr<MultiLibrary> library;
	if (!config.partitions.empty())
		library = MakeLibrary();

	WorkshopSpawnHook hook{library.get()};

	auto spawner = LaunchSpawnServer(config.spawn, &hook);

	Instance instance{
		config,
		std::move(spawner.socket),
		spawner.cgroup.IsDefined(),
		std::move(library),
	};

	spawner = {}; // close the pidfd

#ifdef HAVE_LIBCAP
	/* now that the spawner has been launched by the Instance
	   constructor, drop all capabilities, we don't need any */
	CapabilityState::Empty().Install();
#endif // HAVE_LIBCAP

	instance.Start();

#ifdef HAVE_LIBSYSTEMD
	/* tell systemd we're ready */
	sd_notify(0, "READY=1");
#endif

	/* main loop */

	instance.Run();
}

int
main(int argc, char **argv)
try {
	if (!IsKernelVersionOrNewer({5, 12}))
		throw "Your Linux kernel is too old; this program requires at least 5.12";

	if (geteuid() == 0)
		throw "Refusing to run as root";

	InitProcessName(argc, argv);

#ifndef NDEBUG
	/* also checking $SYSTEMD_EXEC_PID to see if we were launched
	   by systemd, because if Workshop is running in a container,
	   it may not have CAP_SYS_ADMIN */
	debug_mode =
#ifdef HAVE_LIBCAP
		!IsSysAdmin() &&
#endif // HAVE_LIBCAP
		getenv("SYSTEMD_EXEC_PID") == nullptr;
#endif

	Config config;

	/* configuration */

	ParseCommandLine(argc, argv);
	LoadConfigFile(config, "/etc/cm4all/workshop/workshop.conf");

	DropInheritableCapabilities();

	config.Check();

	/* set up */

	Run(config);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
