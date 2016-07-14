/*
 * Manage operator processes.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Operator.hxx"
#include "Workplace.hxx"
#include "Plan.hxx"
#include "Job.hxx"

extern "C" {
#include "syslog.h"
}

#include <daemon/log.h>

#include <map>
#include <string>

#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>

Operator::~Operator()
{
    if (stdout_fd >= 0) {
        stdout_event.Delete();
        close(stdout_fd);
    }

    if (stderr_fd >= 0) {
        stderr_event.Delete();
        close(stderr_fd);
    }

    if (syslog != nullptr)
        syslog_close(&syslog);
}

void
Operator::OnOutputReady(short)
{
    char buffer[512];
    ssize_t nbytes, i;
    unsigned new_progress = 0, p;

    nbytes = read(stdout_fd, buffer, sizeof(buffer));
    if (nbytes <= 0) {
        stdout_event.Delete();
        close(stdout_fd);
        stdout_fd = -1;
        return;
    }

    for (i = 0; i < nbytes; ++i) {
        char ch = buffer[i];

        if (ch >= '0' && ch <= '9' &&
            stdout_length < sizeof(stdout_buffer) - 1) {
            stdout_buffer[stdout_length++] = ch;
        } else {
            if (stdout_length > 0) {
                stdout_buffer[stdout_length] = 0;
                p = (unsigned)strtoul(stdout_buffer, nullptr, 10);
                if (p <= 100)
                    new_progress = p;
            }

            stdout_length = 0;
        }
    }

    if (new_progress > 0 && new_progress != progress) {
        job.SetProgress(new_progress, plan->timeout.c_str());
        progress = new_progress;
    }
}

void
Operator::SetOutput(int fd)
{
    assert(fd >= 0);
    assert(stdout_fd < 0);

    stdout_fd = fd;
    stdout_event.Set(fd, EV_READ|EV_PERSIST);
    stdout_event.Add();

}

void
Operator::OnErrorReady(short)
{
    assert(syslog != nullptr);

    char buffer[512];
    ssize_t nbytes = read(stderr_fd, buffer, sizeof(buffer));
    if (nbytes <= 0) {
        stderr_event.Delete();
        close(stderr_fd);
        stderr_fd = -1;
        return;
    }

    for (ssize_t i = 0; i < nbytes; ++i) {
        char ch = buffer[i];

        if (ch == '\r' || ch == '\n') {
            if (stderr_length > 0) {
                stderr_buffer[stderr_length] = 0;
                syslog_log(syslog, 6, stderr_buffer);
            }

            stderr_length = 0;
        } else if (ch > 0 && (ch & ~0x7f) == 0 &&
                   stderr_length < sizeof(stderr_buffer) - 1) {
            stderr_buffer[stderr_length++] = ch;
        }
    }
}

void
Operator::SetSyslog(int fd)
{
    assert(fd >= 0);
    assert(stderr_fd < 0);

    stderr_fd = fd;
    stderr_event.Set(stderr_fd, EV_READ|EV_PERSIST);
    stderr_event.Add();
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
    vars.insert(std::make_pair("NODE", workplace.GetNodeName()));
    vars.insert(std::make_pair("JOB", job.id));
    vars.insert(std::make_pair("PLAN", job.plan_name));

    for (auto &i : args)
        expand_vars(i, vars);
}

void
Operator::OnProcessExit(int status)
{
    int exit_status = WEXITSTATUS(status);

    if (WIFSIGNALED(status)) {
        daemon_log(1, "job %s (pid %d) died from signal %d%s\n",
                   job.id.c_str(), pid,
                   WTERMSIG(status),
                   WCOREDUMP(status) ? " (core dumped)" : "");
        exit_status = -1;
    } else if (exit_status == 0)
        daemon_log(3, "job %s (pid %d) exited with success\n",
                   job.id.c_str(), pid);
    else
        daemon_log(2, "job %s (pid %d) exited with status %d\n",
                   job.id.c_str(), pid,
                   exit_status);

    job.SetDone(exit_status);
}
