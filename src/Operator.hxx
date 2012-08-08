/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_OPERATOR_HXX
#define WORKSHOP_OPERATOR_HXX

#include "Event.hxx"
#include "Job.hxx"

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
    Job job;
    Plan *plan;
    pid_t pid;

    int stdout_fd = -1;
    Event stdout_event;
    char stdout_buffer[64];
    size_t stdout_length = 0;
    unsigned progress = 0;

    int stderr_fd = -1;
    Event stderr_event;
    char stderr_buffer[512];
    size_t stderr_length = 0;
    struct syslog_client *syslog = nullptr;

    Operator(Workplace *_workplace, const Job &_job,
             Plan *_plan)
        :workplace(_workplace), job(_job), plan(_plan),
         stdout_event([this](int,short){ OnOutputReady(); }),
         stderr_event([this](int,short){ OnErrorReady(); })
    {}

#if 0
    Operator(Operator &&other)
        :workplace(other.workplace), job(other.job), plan(other.plan),
         pid(other.pid),
         syslog(other.syslog) {
        assert(other.stdout_fd < 0);
        assert(other.stdout_length == 0);
        assert(other.stderr_fd < 0);
        assert(other.stderr_length == 0);
    }
#endif

    Operator(const Operator &other) = delete;

    ~Operator();

    Operator &operator=(const Operator &other) = delete;

    void SetOutput(int fd);
    void SetSyslog(int fd);

    void Expand(std::list<std::string> &args) const;

private:
    void OnOutputReady();
    void OnErrorReady();
};

#endif
