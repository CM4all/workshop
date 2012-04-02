/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_WORKPLACE_H
#define WORKSHOP_WORKPLACE_H

struct plan;
struct job;

struct workplace {
    const char *node_name;
    struct Operator *head;
    unsigned max_operators, num_operators;
    char *plan_names, *full_plan_names;
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
