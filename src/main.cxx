/*
 * cm4all-workshop's main().
 *
 * author: Max Kellermann <mk@cm4all.com>
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

    LogConcat(1, nullptr, "cm4all-workshop v" VERSION);

    instance.UpdateLibraryAndFilter(true);

    /* tell systemd we're ready */
    sd_notify(0, "READY=1");

    /* main loop */

    instance.Dispatch();

    /* cleanup */

    LogConcat(5, nullptr, "cleaning up");
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

    LogConcat(4, nullptr, "exiting");
    return EXIT_SUCCESS;
} catch (const std::exception &e) {
    PrintException(e);
    return EXIT_FAILURE;
}
