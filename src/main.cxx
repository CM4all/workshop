/*
 * cm4all-workshop's main().
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "debug.h"
#include "CommandLine.hxx"
#include "Instance.hxx"
#include "Library.hxx"
#include "Queue.hxx"
#include "Job.hxx"
#include "Workplace.hxx"
#include "event/Loop.hxx"
#include "system/SetupProcess.hxx"
#include "version.h"

#include <inline/compiler.h>
#include <daemon/log.h>

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
Run(const Config &config)
{
    SetupProcess();

    if (daemon_user_set(&config.user) < 0)
        exit(2);

    Instance instance("/etc/cm4all/workshop/plans",
                      config.node_name, config.database, "",
                      config.concurrency);

    setup_signal_handlers();

    daemon_log(1, "cm4all-workshop v" VERSION "\n");

    /* main loop */

    instance.UpdateLibraryAndFilter();

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
        Run(config);
    } catch (const std::exception &e) {
        daemon_log(2, "%s\n", e.what());
        return EXIT_FAILURE;
    }

    daemon_log(4, "exiting\n");
    return EXIT_SUCCESS;
}
