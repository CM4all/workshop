/*
 * cm4all-workshop's main().
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "debug.h"
#include "Instance.hxx"
#include "CommandLine.hxx"
#include "Config.hxx"
#include "Library.hxx"
#include "Queue.hxx"
#include "Job.hxx"
#include "Workplace.hxx"
#include "event/Loop.hxx"
#include "system/SetupProcess.hxx"

#include <inline/compiler.h>
#include <daemon/log.h>

#include <systemd/sd-daemon.h>

#include <stdexcept>

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <grp.h>

#ifndef NDEBUG
bool debug_mode = false;
#endif

static void
setup_signal_handlers()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, nullptr);
    sigaction(SIGUSR1, &sa, nullptr);
    sigaction(SIGUSR2, &sa, nullptr);
}

static void
Run(int argc, char **argv, const Config &config)
{
    SetupProcess();

    setup_signal_handlers();

    Instance instance("/etc/cm4all/workshop/plans",
                      config, "",
                      [argc, argv](){
                          /* rename the process */
                          size_t name_size = strlen(argv[0]);
                          for (int i = 0; i < argc; ++i)
                              memset(argv[i], 0, strlen(argv[i]));
                          strncpy(argv[0], "spawn", name_size);
                      });

    if (daemon_user_set(&config.user) < 0)
        exit(2);

    instance.Start();

    daemon_log(1, "cm4all-workshop v" VERSION "\n");

    instance.UpdateLibraryAndFilter(true);

    /* tell systemd we're ready */
    sd_notify(0, "READY=1");

    /* main loop */

    instance.event_loop.Dispatch();

    /* cleanup */

    daemon_log(5, "cleaning up\n");
}

int main(int argc, char **argv) {
    Config config;

#ifndef NDEBUG
    if (geteuid() != 0)
        debug_mode = true;
#endif

    /* configuration */

    parse_cmdline(config, argc, argv);

    /* set up */

    try {
        Run(argc, argv, config);
    } catch (const std::exception &e) {
        daemon_log(2, "%s\n", e.what());
        return EXIT_FAILURE;
    }

    daemon_log(4, "exiting\n");
    return EXIT_SUCCESS;
}
