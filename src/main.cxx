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
exit_callback(gcc_unused int fd, gcc_unused short event, void *arg)
{
    Instance &instance = *(Instance *)arg;

    if (instance.should_exit)
        return;

    instance.should_exit = true;
    event_del(&instance.sigterm_event);
    event_del(&instance.sigint_event);
    event_del(&instance.sigquit_event);
    event_del(&instance.sighup_event);

    instance.queue->Disable();

    if (instance.workplace != NULL) {
        if (instance.workplace->IsEmpty()) {
            event_del(&instance.sigchld_event);

            delete instance.workplace;
            instance.workplace = NULL;

            if (instance.queue != NULL) {
                delete instance.queue;
                instance.queue = NULL;
            }
        } else {
            daemon_log(1, "waiting for operators to finish\n");
        }
    }
}

static void
update_filter(Instance &instance)
{
    instance.queue->SetFilter(instance.library->GetPlanNames(),
                               instance.workplace->GetFullPlanNames(),
                               instance.workplace->GetRunningPlanNames());
}

static void
update_library_and_filter(Instance &instance)
{
    instance.library->Update();
    update_filter(instance);
}

static void
reload_callback(gcc_unused int fd, gcc_unused short event, void *arg)
{
    Instance &instance = *(Instance *)arg;

    if (instance.queue == NULL)
        return;

    daemon_log(4, "reloading\n");
    update_library_and_filter(instance);
    instance.queue->Reschedule();
}

static void
child_callback(gcc_unused int fd, gcc_unused short event, void *arg)
{
    Instance &instance = *(Instance *)arg;

    if (instance.workplace == NULL)
        return;

    instance.workplace->WaitPid();

    if (instance.should_exit) {
        if (instance.workplace->IsEmpty()) {
            event_del(&instance.sigchld_event);

            delete instance.workplace;
            instance.workplace = NULL;

            if (instance.queue != NULL) {
                delete instance.queue;
                instance.queue = NULL;
            }
        }
    } else {
        update_library_and_filter(instance);

        if (!instance.workplace->IsFull())
            instance.queue->Enable();
    }
}

static void
setup_signal_handlers(Instance &instance)
{
    struct sigaction sa;

    event_set(&instance.sigterm_event, SIGTERM, EV_SIGNAL|EV_PERSIST,
              exit_callback, &instance);
    event_add(&instance.sigterm_event, NULL);

    event_set(&instance.sigint_event, SIGINT, EV_SIGNAL|EV_PERSIST,
              exit_callback, &instance);
    event_add(&instance.sigint_event, NULL);

    event_set(&instance.sigquit_event, SIGQUIT, EV_SIGNAL|EV_PERSIST,
              exit_callback, &instance);
    event_add(&instance.sigquit_event, NULL);

    event_set(&instance.sighup_event, SIGHUP, EV_SIGNAL|EV_PERSIST,
              reload_callback, &instance);
    event_add(&instance.sighup_event, NULL);

    event_set(&instance.sigchld_event, SIGCHLD, EV_SIGNAL|EV_PERSIST,
              child_callback, &instance);
    event_add(&instance.sigchld_event, NULL);

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

static void queue_callback(Job *job, void *ctx) {
    Instance &instance = *(Instance *)ctx;

    if (instance.workplace->IsFull()) {
        job_rollback(&job);
        instance.queue->Disable();
        return;
    }

    instance.library->Update();

    if (!start_job(instance, job) || instance.workplace->IsFull())
        instance.queue->Disable();

    update_filter(instance);
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

    int ret = queue_open(config.node_name, config.database,
                         queue_callback, &instance,
                         &instance.queue);
    if (ret != 0) {
        fprintf(stderr, "failed to open queue database\n");
        exit(2);
    }

    instance.workplace = new Workplace(config.node_name, config.concurrency);

    setup_signal_handlers(instance);

    ret = daemonize();
    if (ret < 0)
        exit(2);

    daemon_log(1, "cm4all-workshop v" VERSION "\n");

    daemon_log(4, "using libevent %s method '%s' for polling\n",
               event_get_version(), event_get_method());

    /* main loop */

    update_library_and_filter(instance);

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

    Run(config);

    daemonize_cleanup();

    config_dispose(&config);

    daemon_log(4, "exiting\n");
}
