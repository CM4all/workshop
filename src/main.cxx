/*
 * cm4all-workshop's main().
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "debug.h"

extern "C" {
#include "cmdline.h"
}

#include "plan.hxx"
#include "queue.hxx"
#include "workplace.hxx"
#include "version.h"

#include <daemon/log.h>
#include <daemon/daemonize.h>

#include <glib.h>
#include <event.h>

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <grp.h>

#ifndef NDEBUG
int debug_mode = 0;
#endif

struct instance {
    struct library *library;
    struct queue *queue;
    struct workplace *workplace;
    int should_exit;
    struct event sigterm_event, sigint_event, sigquit_event;
    struct event sighup_event, sigchld_event;
};

static void config_get(struct config *config, int argc, char **argv) {
    memset(config, 0, sizeof(*config));
    config->concurrency = 2;

    /*config_read_file(config, "/etc/cm4all/workshop/workshop.conf");*/

    parse_cmdline(config, argc, argv);
}

static void
exit_callback(G_GNUC_UNUSED int fd, G_GNUC_UNUSED short event, void *arg)
{
    struct instance *instance = (struct instance*)arg;

    if (instance->should_exit)
        return;

    instance->should_exit = 1;
    event_del(&instance->sigterm_event);
    event_del(&instance->sigint_event);
    event_del(&instance->sigquit_event);
    event_del(&instance->sighup_event);

    queue_disable(instance->queue);

    if (instance->workplace != NULL) {
        if (workplace_is_empty(instance->workplace)) {
            event_del(&instance->sigchld_event);
            workplace_close(&instance->workplace);
            if (instance->queue != NULL)
                queue_close(&instance->queue);
        } else {
            daemon_log(1, "waiting for operators to finish\n");
        }
    }
}

static void update_filter(struct instance *instance) {
    queue_set_filter(instance->queue,
                     library_plan_names(instance->library),
                     workplace_full_plan_names(instance->workplace),
                     workplace_plan_names(instance->workplace));
}

static void update_library_and_filter(struct instance *instance) {
    library_update(instance->library);
    update_filter(instance);
}

static void
reload_callback(G_GNUC_UNUSED int fd, G_GNUC_UNUSED short event, void *arg)
{
    struct instance *instance = (struct instance*)arg;

    if (instance->queue == NULL)
        return;

    daemon_log(4, "reloading\n");
    update_library_and_filter(instance);
    queue_reschedule(instance->queue);
}

static void
child_callback(G_GNUC_UNUSED int fd, G_GNUC_UNUSED short event, void *arg)
{
    struct instance *instance = (struct instance*)arg;

    if (instance->workplace == NULL)
        return;

    workplace_waitpid(instance->workplace);

    if (instance->should_exit) {
        if (workplace_is_empty(instance->workplace)) {
            event_del(&instance->sigchld_event);
            workplace_close(&instance->workplace);
            if (instance->queue != NULL)
                queue_close(&instance->queue);
        }
    } else {
        update_library_and_filter(instance);

        if (!workplace_is_full(instance->workplace))
            queue_enable(instance->queue);
    }
}

static void setup_signal_handlers(struct instance *instance) {
    struct sigaction sa;

    event_set(&instance->sigterm_event, SIGTERM, EV_SIGNAL|EV_PERSIST,
              exit_callback, instance);
    event_add(&instance->sigterm_event, NULL);

    event_set(&instance->sigint_event, SIGINT, EV_SIGNAL|EV_PERSIST,
              exit_callback, instance);
    event_add(&instance->sigint_event, NULL);

    event_set(&instance->sigquit_event, SIGQUIT, EV_SIGNAL|EV_PERSIST,
              exit_callback, instance);
    event_add(&instance->sigquit_event, NULL);

    event_set(&instance->sighup_event, SIGHUP, EV_SIGNAL|EV_PERSIST,
              reload_callback, instance);
    event_add(&instance->sighup_event, NULL);

    event_set(&instance->sigchld_event, SIGCHLD, EV_SIGNAL|EV_PERSIST,
              child_callback, instance);
    event_add(&instance->sigchld_event, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
}

static int start_job(struct instance *instance, struct job *job) {
    int ret;
    struct plan *plan;

    ret = library_get(instance->library, job->plan_name, &plan);
    if (ret != 0) {
        fprintf(stderr, "library_get('%s') failed\n", job->plan_name);
        job_rollback(&job);
        return ret;
    }

    ret = job_set_progress(job, 0, plan->timeout);
    if (ret < 0) {
        job_rollback(&job);
        return ret;
    }

    ret = workplace_start(instance->workplace, job, plan);
    if (ret != 0) {
        plan_put(&plan);
        job_done(&job, -1);
    }

    return 0;
}

static void queue_callback(struct job *job, void *ctx) {
    struct instance *instance = (struct instance*)ctx;
    int ret;

    if (workplace_is_full(instance->workplace)) {
        job_rollback(&job);
        queue_disable(instance->queue);
        return;
    }

    library_update(instance->library);

    ret = start_job(instance, job);
    if (ret != 0 || workplace_is_full(instance->workplace))
        queue_disable(instance->queue);

    update_filter(instance);
}

int main(int argc, char **argv) {
    struct config config;
    struct instance instance;
    int ret;

#ifndef NDEBUG
    if (geteuid() != 0)
        debug_mode = 1;
#endif

    /* configuration */

    config_get(&config, argc, argv);

    /* set up */

    memset(&instance, 0, sizeof(instance));

    ret = daemonize_prepare();
    if (ret < 0)
        exit(2);

    ret = library_open("/etc/cm4all/workshop/plans", &instance.library);
    if (ret != 0) {
        fprintf(stderr, "library_open() failed\n");
        exit(2);
    }

    event_init();

    ret = queue_open(config.node_name, config.database,
                     queue_callback, &instance,
                     &instance.queue);
    if (ret != 0) {
        fprintf(stderr, "failed to open queue database\n");
        exit(2);
    }

    ret = workplace_open(config.node_name, config.concurrency,
                         &instance.workplace);
    if (ret != 0) {
        fprintf(stderr, "failed to open workplace\n");
        exit(2);
    }

    setup_signal_handlers(&instance);

    ret = daemonize();
    if (ret < 0)
        exit(2);

    daemon_log(1, "cm4all-workshop v" VERSION "\n");

    daemon_log(4, "using libevent %s method '%s' for polling\n",
               event_get_version(), event_get_method());

    /* main loop */

    update_library_and_filter(&instance);

    event_dispatch();

    /* cleanup */

    daemon_log(5, "cleaning up\n");

    if (instance.workplace != NULL)
        workplace_close(&instance.workplace);

    if (instance.queue != NULL)
        queue_close(&instance.queue);

    library_close(&instance.library);

    daemonize_cleanup();

    config_dispose(&config);

    daemon_log(4, "exiting\n");
}