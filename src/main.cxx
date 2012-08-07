/*
 * cm4all-workshop's main().
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "debug.h"

extern "C" {
#include "cmdline.h"
}

#include "Instance.hxx"
#include "Library.hxx"
#include "Queue.hxx"
#include "Job.hxx"
#include "Workplace.hxx"
#include "version.h"

#include <inline/compiler.h>
#include <daemon/log.h>
#include <daemon/daemonize.h>

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

static void config_get(struct config *config, int argc, char **argv) {
    memset(config, 0, sizeof(*config));
    config->concurrency = 2;

    /*config_read_file(config, "/etc/cm4all/workshop/workshop.conf");*/

    parse_cmdline(config, argc, argv);
}

static void
setup_signal_handlers()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
}

static bool
start_job(Instance &instance, Job *job)
{
    Plan *plan = instance.library->Get(job->plan_name.c_str());
    if (plan == nullptr) {
        fprintf(stderr, "library_get('%s') failed\n", job->plan_name.c_str());
        job_rollback(&job);
        return false;
    }

    int ret = job->SetProgress(0, plan->timeout.c_str());
    if (ret < 0) {
        job_rollback(&job);
        return false;
    }

    ret = instance.workplace->Start(job, plan);
    if (ret != 0) {
        plan_put(&plan);
        job_done(&job, -1);
    }

    return true;
}

static void
queue_callback(Instance &instance, Job *job)
{
    if (instance.workplace->IsFull()) {
        job_rollback(&job);
        instance.queue->Disable();
        return;
    }

    instance.library->Update();

    if (!start_job(instance, job) || instance.workplace->IsFull())
        instance.queue->Disable();

    instance.UpdateFilter();
}

static void
Run(struct config &config)
{
    EventBase event_base;

    Instance instance;
    instance.library = Library::Open("/etc/cm4all/workshop/plans");
    if (instance.library == nullptr) {
        fprintf(stderr, "library_open() failed\n");
        exit(2);
    }

    instance.queue = new Queue(config.node_name, config.database,
                               [&instance](Job *job) {
                                   queue_callback(instance, job);
                               });

    instance.workplace = new Workplace(config.node_name, config.concurrency);

    setup_signal_handlers();

    if (daemonize() < 0)
        exit(2);

    daemon_log(1, "cm4all-workshop v" VERSION "\n");

    daemon_log(4, "using libevent %s method '%s' for polling\n",
               event_get_version(), event_get_method());

    /* main loop */

    instance.UpdateLibraryAndFilter();

    event_base.Dispatch();

    /* cleanup */

    daemon_log(5, "cleaning up\n");

    delete instance.workplace;
    delete instance.queue;

    delete instance.library;
}

int main(int argc, char **argv) {
    struct config config;

#ifndef NDEBUG
    if (geteuid() != 0)
        debug_mode = true;
#endif

    /* configuration */

    config_get(&config, argc, argv);

    /* set up */

    if (daemonize_prepare() < 0)
        exit(2);

    try {
        Run(config);
    } catch (const std::exception &e) {
        daemon_log(2, "%s\n", e.what());
    }

    daemonize_cleanup();

    config_dispose(&config);

    daemon_log(4, "exiting\n");
}
