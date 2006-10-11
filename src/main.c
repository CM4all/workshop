/*
 * $Id$
 *
 * cm4all-workshop's main().
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "workshop.h"

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

struct instance {
    struct library *library;
    struct queue *queue;
    struct poll *poll;
    struct workplace *workplace;
};

static void config_get(struct config *config, int argc, char **argv) {
    memset(config, 0, sizeof(*config));
    config->concurrency = 2;

    /*config_read_file(config, "/etc/cm4all/workshop/workshop.conf");*/

    parse_cmdline(config, argc, argv);
}

static int should_exit = 0, child_exited = 0;

static void exit_signal_handler(int sig) {
    (void)sig;
    should_exit = 1;
}

static void child_signal_handler(int sig) {
    (void)sig;
    child_exited = 1;
}

static void setup_signal_handlers(void) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = exit_signal_handler;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = child_signal_handler;
    sigaction(SIGCHLD, &sa, NULL);
}

static int start_job(struct instance *instance, struct job *job,
                     struct plan *plan) {
    int ret;

    ret = workplace_start(instance->workplace, job, plan);
    if (ret != 0) {
        plan_put(&plan);
        job_done(&job, -1);
    }

    return 0;
}

static void claim_and_start_job(struct instance *instance, struct job *job) {
    int ret;
    struct plan *plan;

    ret = library_get(instance->library, job->plan_name, &plan);
    if (ret != 0) {
        fprintf(stderr, "library_get() failed\n");
        job_skip(&job);
        return;
    }

    ret = job_claim(&job, plan->timeout);
    if (ret <= 0) {
        plan_put(&plan);
        return;
    }

    ret = start_job(instance, job, plan);
    if (ret != 0)
        job_rollback(&job);
}

int main(int argc, char **argv) {
    struct config config;
    struct instance instance;
    int ret;

    /* configuration */

    config_get(&config, argc, argv);

    log(1, "cm4all-workshop v" VERSION "\n");

    /* set up */

    memset(&instance, 0, sizeof(instance));

    ret = library_open("/etc/cm4all/workshop/plans", &instance.library);
    if (ret != 0) {
        fprintf(stderr, "library_open() failed\n");
        exit(2);
    }

    ret = poll_open(&instance.poll);
    if (ret != 0) {
        fprintf(stderr, "poll_open() failed\n");
        exit(2);
    }

    ret = queue_open(config.node_name, config.database,
                     instance.poll, &instance.queue);
    if (ret != 0) {
        fprintf(stderr, "failed to open queue database\n");
        exit(2);
    }

    ret = workplace_open(config.node_name, config.concurrency,
                         instance.poll,
                         &instance.workplace);
    if (ret != 0) {
        fprintf(stderr, "failed to open workplace\n");
        exit(2);
    }

    setup_signal_handlers();

    stdin_null();

    daemonize(&config);

    /* main loop */

    while (!should_exit || !workplace_is_empty(instance.workplace)) {
        /* handle job queue */

        while (!should_exit && !workplace_is_full(instance.workplace)) {
            struct job *job;

            ret = queue_get(instance.queue, &job);
            if (ret <= 0)
                break;

            claim_and_start_job(&instance, job);
        }

        queue_flush(instance.queue);

        /* poll file handles */

        poll_poll(instance.poll);

        /* check child processes */

        if (child_exited) {
            child_exited = 0;
            workplace_waitpid(instance.workplace);
        }

        /* informational message */

        if (should_exit == 1 && !workplace_is_empty(instance.workplace)) {
            should_exit = 2;
            log(1, "waiting for operators to finish\n");
        }
    }

    /* cleanup */

    workplace_close(&instance.workplace);

    queue_close(&instance.queue);

    poll_close(&instance.poll);

    library_close(&instance.library);

    config_dispose(&config);
}
