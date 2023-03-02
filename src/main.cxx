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
#include "system/SetupProcess.hxx"
#include "system/ProcessName.hxx"
#include "lib/cap/Glue.hxx"
#include "lib/cap/State.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/PrintException.hxx"

#include <systemd/sd-daemon.h>

#include <stdexcept>

#include <stdio.h>
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
Run(const Config &config)
{
	SetupProcess();

	std::unique_ptr<MultiLibrary> library;
	if (!config.partitions.empty())
		library = MakeLibrary();

	WorkshopSpawnHook hook{library.get()};

	auto spawner_socket = LaunchSpawnServer(config.spawn, &hook);

	Instance instance{
		config,
		std::move(spawner_socket),
		std::move(library),
	};

	/* now that the spawner has been launched by the Instance
	   constructor, drop all capabilities, we don't need any */
	CapabilityState::Empty().Install();

	instance.Start();

	/* tell systemd we're ready */
	sd_notify(0, "READY=1");

	/* main loop */

	instance.Run();
}

int
main(int argc, char **argv)
try {
	if (geteuid() == 0)
		throw "Refusing to run as root";

	InitProcessName(argc, argv);

#ifndef NDEBUG
	debug_mode = !IsSysAdmin();
#endif

	Config config;

	/* configuration */

	ParseCommandLine(argc, argv);
	LoadConfigFile(config, "/etc/cm4all/workshop/workshop.conf");

	config.Check();

	/* set up */

	Run(config);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
