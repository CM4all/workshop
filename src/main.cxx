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
#include "util/PrintException.hxx"

#include <daemon/log.h>

#include <systemd/sd-daemon.h>

#include <stdexcept>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NDEBUG
bool debug_mode = false;
#endif

static void
Run(int argc, char **argv, const Config &config)
{
    SetupProcess();

    Instance instance(config,
                      [argc, argv](){
                          /* rename the process */
                          size_t name_size = strlen(argv[0]);
                          for (int i = 0; i < argc; ++i)
                              memset(argv[i], 0, strlen(argv[i]));
                          strncpy(argv[0], "spawn", name_size);
                      });
    instance.InsertLibraryPath("/etc/cm4all/workshop/plans");
    instance.InsertLibraryPath("/usr/share/cm4all/workshop/plans");

    config.user.Apply();

    instance.Start();

    daemon_log(1, "cm4all-workshop v" VERSION "\n");

    instance.UpdateLibraryAndFilter(true);

    /* tell systemd we're ready */
    sd_notify(0, "READY=1");

    /* main loop */

    instance.Dispatch();

    /* cleanup */

    daemon_log(5, "cleaning up\n");
}

int
main(int argc, char **argv)
try {
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

    Run(argc, argv, config);

    daemon_log(4, "exiting\n");
    return EXIT_SUCCESS;
} catch (const std::exception &e) {
    PrintException(e);
    return EXIT_FAILURE;
}
