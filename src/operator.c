/*
 * $Id$
 *
 * Manage operator processes.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "workshop.h"
#include "syslog.h"

#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <poll.h>

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

    if (operator->stdout_fd >= 0) {
        poll_remove(operator->workplace->poll, operator->stdout_fd);
        close(operator->stdout_fd);
    }

    if (operator->stderr_fd >= 0) {
        poll_remove(operator->workplace->poll, operator->stderr_fd);
        close(operator->stderr_fd);
    }

    if (operator->syslog != NULL)
        syslog_close(&operator->syslog);

    free(operator);
}

static void stdout_callback(struct pollfd *pollfd, void *ctx) {
    struct operator *operator = (struct operator*)ctx;
    char buffer[512];
    ssize_t nbytes, i;
    unsigned progress = 0, p;

    (void)pollfd;

    nbytes = read(operator->stdout_fd, buffer, sizeof(buffer));
    if (nbytes <= 0) {
        poll_remove(operator->workplace->poll, operator->stdout_fd);
        close(operator->stdout_fd);
        operator->stdout_fd = -1;
        return;
    }

    for (i = 0; i < nbytes; ++i) {
        char ch = buffer[i];

        if (ch == '\r' || ch == '\n') {
            if (operator->stdout_length > 0) {
                operator->stdout_buffer[operator->stdout_length] = 0;
                p = (unsigned)strtoul(operator->stdout_buffer, NULL, 10);
                if (p <= 100)
                    progress = p;
            }

            operator->stdout_length = 0;
        } else if (ch >= '0' && ch <= '9' &&
                   operator->stdout_length < sizeof(operator->stdout_buffer) - 1) {
            operator->stdout_buffer[operator->stdout_length++] = ch;
        }
    }

    if (progress > 0) {
        job_set_progress(operator->job, progress);
        fprintf(stderr, "progress=%u\n", progress);
    }
}

static void stderr_callback(struct pollfd *pollfd, void *ctx) {
    struct operator *operator = (struct operator*)ctx;
    char buffer[512];
    ssize_t nbytes, i;

    (void)pollfd;

    assert(operator->syslog != NULL);

    nbytes = read(operator->stderr_fd, buffer, sizeof(buffer));
    if (nbytes <= 0) {
        poll_remove(operator->workplace->poll, operator->stderr_fd);
        close(operator->stderr_fd);
        operator->stderr_fd = -1;
        return;
    }

    for (i = 0; i < nbytes; ++i) {
        char ch = buffer[i];

        if (ch == '\r' || ch == '\n') {
            if (operator->stderr_length > 0) {
                operator->stderr_buffer[operator->stderr_length] = 0;
                syslog_log(operator->syslog, 6, operator->stderr_buffer);
            }

            operator->stderr_length = 0;
        } else if (ch > 0 && (ch & ~0x7f) == 0 &&
                   operator->stderr_length < sizeof(operator->stderr_buffer) - 1) {
            operator->stderr_buffer[operator->stderr_length++] = ch;
        }
    }
}

int workplace_start(struct workplace *workplace,
                    struct job *job, struct plan *plan) {
    struct operator *operator;
    int ret, stdout_fds[2], stderr_fds[2];

    assert(plan != NULL);
    assert(plan->argv != NULL);
    assert(plan->argc > 0);

    operator = (struct operator*)calloc(1, sizeof(*operator));
    if (operator == NULL)
        return errno;

    operator->workplace = workplace;
    operator->stdout_fd = -1;
    operator->stderr_fd = -1;

    ret = pipe(stdout_fds);
    if (ret < 0) {
        fprintf(stderr, "pipe() failed: %s\n", strerror(errno));
        free_operator(&operator);
        return -1;
    }

    operator->stdout_fd = stdout_fds[0];
    poll_add(workplace->poll, operator->stdout_fd, POLLIN,
             stdout_callback, operator);

    if (job->syslog_server != NULL) {
        ret = syslog_open("foo", "bar", 1, job->syslog_server, &operator->syslog);
        if (ret != 0) {
            if (ret > 0)
                fprintf(stderr, "syslog_open(%s) failed: %s\n",
                        job->syslog_server, strerror(ret));
            free_operator(&operator);
            close(stdout_fds[1]);
            return -1;
        }

        ret = pipe(stderr_fds);
        if (ret < 0) {
            fprintf(stderr, "pipe() failed: %s\n", strerror(errno));
            free_operator(&operator);
            close(stdout_fds[1]);
            return -1;
        }

        operator->stderr_fd = stderr_fds[0];
        poll_add(workplace->poll, operator->stderr_fd, POLLIN,
                 stderr_callback, operator);
    }

    operator->job = job;
    operator->plan = plan;
    operator->pid = fork();
    if (operator->pid < 0) {
        fprintf(stderr, "fork() failed: %s\n", strerror(errno));
        free_operator(&operator);
        close(stdout_fds[1]);
        if (job->syslog_server != NULL)
            close(stderr_fds[1]);
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

        /* connect pipes */

        dup2(stdout_fds[1], 1);
        if (job->syslog_server != NULL)
            dup2(stderr_fds[1], 2);

        close(stdout_fds[0]);
        close(stdout_fds[1]);
        if (job->syslog_server != NULL) {
            close(stderr_fds[0]);
            close(stderr_fds[1]);
        }

        /* execute plan */

        execv(argv[0], argv);
        fprintf(stderr, "execv() failed: %s\n", strerror(errno));
        exit(1);
    }

    close(stdout_fds[1]);
    if (job->syslog_server != NULL)
        close(stderr_fds[1]);

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

        plan_put(&operator->plan);

        job_done(&operator->job, status);

        *operator_p = operator->next;
        free_operator(&operator);
    }
}
