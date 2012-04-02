/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_OPERATOR_H
#define WORKSHOP_OPERATOR_H

#include <event.h>

struct workplace;

/** an operator is a job being executed */
struct Operator {
    struct Operator *next;
    struct workplace *workplace;
    struct job *job;
    struct plan *plan;
    pid_t pid;

    int stdout_fd;
    struct event stdout_event;
    char stdout_buffer[64];
    size_t stdout_length;
    unsigned progress;

    int stderr_fd;
    struct event stderr_event;
    char stderr_buffer[512];
    size_t stderr_length;
    struct syslog_client *syslog;
};

void
free_operator(struct Operator **operator_r);

int
expand_operator_vars(const struct Operator *o,
                     struct strarray *argv);

#endif
