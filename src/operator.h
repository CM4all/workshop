/*
 * Internal declarations of cm4all-workshop.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_OPERATOR_H
#define WORKSHOP_OPERATOR_H

#include <event.h>

struct workplace;

/** an operator is a job being executed */
struct operator {
    struct operator *next;
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

int workplace_open(const char *node_name, unsigned max_operators,
                   struct workplace **workplace_r);

void workplace_close(struct workplace **workplace_r);

int workplace_plan_is_running(const struct workplace *workplace,
                              const struct plan *plan);

const char *workplace_plan_names(struct workplace *workplace);

/** returns the plan names which have reached their concurrency
    limit */
const char *workplace_full_plan_names(struct workplace *workplace);

int workplace_start(struct workplace *workplace,
                    struct job *job, struct plan *plan);

int workplace_is_empty(const struct workplace *workplace);

int workplace_is_full(const struct workplace *workplace);

void workplace_waitpid(struct workplace *workplace);

#endif