/*
 * Copyright 2006-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "debug.h"
#include "Instance.hxx"
#include "CommandLine.hxx"
#include "Config.hxx"
#include "system/SetupProcess.hxx"
#include "system/ProcessName.hxx"
#include "util/PrintException.hxx"

#include <systemd/sd-daemon.h>

#include <stdexcept>

#include <stdio.h>
#include <stdlib.h>

#ifndef NDEBUG
bool debug_mode = false;
#endif

static void
Run(const Config &config)
{
	SetupProcess();

	Instance instance(config);

	config.user.Apply();

	instance.Start();

	instance.UpdateLibraryAndFilter(true);

	/* tell systemd we're ready */
	sd_notify(0, "READY=1");

	/* main loop */

	instance.Dispatch();
}

int
main(int argc, char **argv)
try {
	InitProcessName(argc, argv);

#ifndef NDEBUG
	if (geteuid() != 0)
		debug_mode = true;
#endif

	Config config;

	/* configuration */

	ParseCommandLine(config, argc, argv);
	LoadConfigFile(config, "/etc/cm4all/workshop/workshop.conf");

	if (config.partitions.empty()) {
		/* compatibility with Workshop 1.0 */
		const char *database = getenv("WORKSHOP_DATABASE");
		if (database != nullptr && *database != 0) {
			config.partitions.emplace_front(database);

			/* compatibility with Workshop 1.0.x: don't require
			   allow_user/allow_group configuration if the
			   configuration has not yet been migrated from
			   /etc/default/cm4all-workshop to workshop.conf */
			config.spawn.allow_any_uid_gid =
				config.spawn.allowed_uids.empty() &&
				config.spawn.allowed_gids.empty();
		}
	}

	config.Check();

	/* set up */

	Run(config);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
