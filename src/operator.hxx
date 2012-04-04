/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_OPERATOR_H
#define WORKSHOP_OPERATOR_H

#include <string>
#include <list>

#include <event.h>

#include <assert.h>

struct Plan;
class Workplace;
struct Job;

/** an operator is a job being executed */
struct Operator {
    Workplace *workplace;
    Job *job;
    Plan *plan;
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

    Operator(Workplace *_workplace, Job *_job,
             Plan *_plan)
        :workplace(_workplace), job(_job), plan(_plan),
         stdout_fd(-1), stdout_length(0),
         progress(0),
         stderr_fd(-1), stderr_length(0),
         syslog(NULL) {}

#if 0
    Operator(Operator &&other)
        :workplace(other.workplace), job(other.job), plan(other.plan),
         pid(other.pid),
         stdout_fd(-1), stdout_length(0),
         progress(0),
         stderr_fd(-1), stderr_length(0),
         syslog(other.syslog) {
        assert(other.stdout_fd < 0);
        assert(other.stdout_length == 0);
        assert(other.stderr_fd < 0);
        assert(other.stderr_length == 0);
    }
#endif

    Operator(const Operator &other) = delete;

    ~Operator();
};

void
free_operator(struct Operator **operator_r);

void
expand_operator_vars(const struct Operator *o,
                     std::list<std::string> &args);

#endif
