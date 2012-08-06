/*
 * Manage operator processes.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "operator.hxx"
#include "workplace.hxx"
#include "plan.hxx"
#include "job.hxx"

extern "C" {
#include "syslog.h"
}

#include <map>
#include <string>

#include <assert.h>
#include <unistd.h>

Operator::~Operator()
{
    if (stdout_fd >= 0) {
        event_del(&stdout_event);
        close(stdout_fd);
    }

    if (stderr_fd >= 0) {
        event_del(&stderr_event);
        close(stderr_fd);
    }

    if (syslog != NULL)
        syslog_close(&syslog);
}

static void
stdout_callback(gcc_unused int fd, gcc_unused short event, void *ctx)
{
    struct Operator *o = (struct Operator*)ctx;
    char buffer[512];
    ssize_t nbytes, i;
    unsigned progress = 0, p;

    nbytes = read(o->stdout_fd, buffer, sizeof(buffer));
    if (nbytes <= 0) {
        event_del(&o->stdout_event);
        close(o->stdout_fd);
        o->stdout_fd = -1;
        return;
    }

    for (i = 0; i < nbytes; ++i) {
        char ch = buffer[i];

        if (ch >= '0' && ch <= '9' &&
            o->stdout_length < sizeof(o->stdout_buffer) - 1) {
            o->stdout_buffer[o->stdout_length++] = ch;
        } else {
            if (o->stdout_length > 0) {
                o->stdout_buffer[o->stdout_length] = 0;
                p = (unsigned)strtoul(o->stdout_buffer, NULL, 10);
                if (p <= 100)
                    progress = p;
            }

            o->stdout_length = 0;
        }
    }

    if (progress > 0 && progress != o->progress) {
        o->job->SetProgress(progress, o->plan->timeout.c_str());
        o->progress = progress;
    }
}

void
Operator::SetOutput(int fd)
{
    assert(fd >= 0);
    assert(stdout_fd < 0);

    stdout_fd = fd;
    event_set(&stdout_event, fd,
              EV_READ|EV_PERSIST, stdout_callback, this);
    event_add(&stdout_event, NULL);

}

typedef std::map<std::string, std::string> StringMap;

static void
expand_vars(std::string &p, const StringMap &vars)
{
    const std::string src = std::move(p);
    p.clear();

    std::string::size_type start = 0, pos;
    while ((pos = src.find("${", start)) != src.npos) {
        std::string::size_type end = src.find('}', start + 2);
        if (end == src.npos)
            break;

        p.append(src.begin() + start, src.begin() + pos);

        const std::string key(src.begin() + start + 2, src.begin() + end);
        auto i = vars.find(key);
        if (i != vars.end())
            p.append(i->second);

        start = end + 1;
    }

    p.append(src.begin() + start, src.end());
}

void
Operator::Expand(std::list<std::string> &args) const
{
    assert(!args.empty());

    StringMap vars;
    vars.insert(std::make_pair("0", args.front()));
    vars.insert(std::make_pair("NODE", workplace->GetNodeName()));
    vars.insert(std::make_pair("JOB", job->id));
    vars.insert(std::make_pair("PLAN", job->plan_name));

    for (auto &i : args)
        expand_vars(i, vars);
}

