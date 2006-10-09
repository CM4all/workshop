/*
 * $Id$
 *
 * Manage operator processes.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "workshop.h"

#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>

struct workplace {
    struct poll *poll;
    struct operator *head;
};

int workplace_open(struct poll *p, struct workplace **workplace_r) {
    struct workplace *workplace;

    workplace = (struct workplace*)calloc(1, sizeof(*workplace));
    if (workplace == NULL)
        return errno;

    workplace->poll = p;

    *workplace_r = workplace;
    return 0;
}

void workplace_close(struct workplace **workplace_r) {
    struct workplace *workplace;

    assert(workplace_r != NULL);
    assert(*workplace_r != NULL);

    workplace = *workplace_r;
    *workplace_r = NULL;

    assert(workplace->head == NULL);

    free(workplace);
}

static void free_operator(struct operator **operator_r) {
    struct operator *operator;

    assert(operator_r != NULL);
    assert(*operator_r != NULL);

    operator = *operator_r;
    *operator_r = NULL;

    free(operator);
}

int workplace_start(struct workplace *workplace,
                    struct queue *queue, struct job *job,
                    struct library *library,
                    struct plan *plan) {
    struct operator *operator;

    assert(plan != NULL);
    assert(plan->argv != NULL);
    assert(plan->argc > 0);

    operator = (struct operator*)calloc(1, sizeof(*operator));
    if (operator == NULL)
        return errno;

    operator->queue = queue;
    operator->job = job;
    operator->library = library;
    operator->plan = plan;
    operator->pid = fork();
    if (operator->pid < 0) {
        fprintf(stderr, "fork() failed: %s\n", strerror(errno));
        free_operator(&operator);
        return -1;
    }

    if (operator->pid == 0) {
        /* in the operator process */

        char **argv;
        unsigned i;

        clearenv();

        argv = calloc(plan->argc + 1, sizeof(argv[0]));
        if (argv == NULL)
            abort();

        for (i = 0; i < plan->argc; ++i)
            argv[i] = plan->argv[i];

        execv(argv[0], argv);
        fprintf(stderr, "execv() failed: %s\n", strerror(errno));
        exit(1);
    }

    operator->next = workplace->head;
    workplace->head = operator;

    return 0;
}

int workplace_is_empty(const struct workplace *workplace) {
    return workplace->head == NULL;
}

static struct operator **find_operator_by_pid(struct workplace *workplace,
                                              pid_t pid) {
    struct operator **operator_p, *operator;

    operator_p = &workplace->head;

    while (1) {
        operator = *operator_p;
        if (operator == NULL)
            return NULL;

        if (operator->pid == pid)
            return operator_p;

        operator_p = &operator->next;
    }
}

void workplace_waitpid(struct workplace *workplace) {
    pid_t pid;
    int status;
    struct operator **operator_p, *operator;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        operator_p = find_operator_by_pid(workplace, pid);
        if (operator_p == NULL)
            continue;

        operator = *operator_p;
        assert(operator != NULL);

        library_put(operator->library, &operator->plan);

        queue_done(operator->queue, &operator->job, status);

        *operator_p = operator->next;
        free_operator(&operator);
    }
}
