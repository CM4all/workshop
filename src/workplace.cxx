/*
 * Manage operator processes.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "workplace.hxx"
#include "operator.hxx"
#include "debug.h"
#include "plan.hxx"
#include "job.hxx"
#include "pg_array.hxx"

extern "C" {
#include "syslog.h"
}

#include <inline/compiler.h>
#include <daemon/log.h>

#include <algorithm>
#include <string>
#include <list>
#include <vector>

#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <poll.h>
#include <grp.h>
#include <sys/time.h>
#include <sys/resource.h>

template<class C, typename V>
static bool
contains(const C &container, const V &value)
{
    return std::find(container.begin(), container.end(), value) != container.end();
}

const char *
Workplace::GetRunningPlanNames()
{
    std::list<std::string> list;
    for (const auto &o : operators)
        if (!contains(list, o->job->plan_name))
            list.push_back(o->job->plan_name);

    plan_names = pg_encode_array(list);
    return plan_names.c_str();
}

struct plan_counter {
    const Plan *plan;
    const char *plan_name;
    unsigned num;
};

const char *
Workplace::GetFullPlanNames()
{
    struct plan_counter *counters;
    unsigned num_counters = 0, i;

    if (num_operators == 0)
        return "{}";

    counters = (struct plan_counter *)
        calloc(num_operators, sizeof(counters[0]));
    if (counters == NULL)
        abort();

    std::list<std::string> list;
    for (const auto &o : operators) {
        if (o->plan->concurrency == 0)
            continue;

        for (i = 0; i < num_counters; ++i)
            if (counters[i].plan == o->plan)
                break;

        if (i == num_counters) {
            ++num_counters;
            counters[i].plan = o->plan;
            counters[i].plan_name = o->job->plan_name.c_str();
        }

        ++counters[i].num;

        assert(counters[i].plan->concurrency == 0 ||
               counters[i].num <= counters[i].plan->concurrency ||
               contains(list, counters[i].plan_name));

        if (counters[i].num == counters[i].plan->concurrency)
            list.push_back(counters[i].plan_name);
    }

    free(counters);

    full_plan_names = pg_encode_array(list);
    return full_plan_names.c_str();
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
        job_set_progress(o->job, progress, o->plan->timeout.c_str());
        o->progress = progress;
    }
}

static void
stderr_callback(gcc_unused int fd, gcc_unused short event, void *ctx)
{
    struct Operator *o = (struct Operator*)ctx;
    char buffer[512];
    ssize_t nbytes, i;

    assert(o->syslog != NULL);

    nbytes = read(o->stderr_fd, buffer, sizeof(buffer));
    if (nbytes <= 0) {
        event_del(&o->stderr_event);
        close(o->stderr_fd);
        o->stderr_fd = -1;
        return;
    }

    for (i = 0; i < nbytes; ++i) {
        char ch = buffer[i];

        if (ch == '\r' || ch == '\n') {
            if (o->stderr_length > 0) {
                o->stderr_buffer[o->stderr_length] = 0;
                syslog_log(o->syslog, 6, o->stderr_buffer);
            }

            o->stderr_length = 0;
        } else if (ch > 0 && (ch & ~0x7f) == 0 &&
                   o->stderr_length < sizeof(o->stderr_buffer) - 1) {
            o->stderr_buffer[o->stderr_length++] = ch;
        }
    }
}

int
Workplace::Start(Job *job, Plan *plan)
{
    int ret, stdout_fds[2], stderr_fds[2];

    assert(!plan->args.empty());

    /* create operator object */

    Operator *o = new Operator(this, job, plan);

    ret = pipe(stdout_fds);
    if (ret < 0) {
        fprintf(stderr, "pipe() failed: %s\n", strerror(errno));
        free_operator(&o);
        return -1;
    }

    /* create stdout/stderr pipes */

    o->stdout_fd = stdout_fds[0];
    event_set(&o->stdout_event, o->stdout_fd,
              EV_READ|EV_PERSIST, stdout_callback, (void *)o);
    event_add(&o->stdout_event, NULL);

    if (!job->syslog_server.empty()) {
        char ident[256];

        snprintf(ident, sizeof(ident), "%s[%s]",
                 job->plan_name.c_str(), job->id.c_str());

        ret = syslog_open(node_name.c_str(), ident, 1,
                          job->syslog_server.c_str(),
                          &o->syslog);
        if (ret != 0) {
            if (ret > 0)
                fprintf(stderr, "syslog_open(%s) failed: %s\n",
                        job->syslog_server.c_str(), strerror(ret));
            free_operator(&o);
            close(stdout_fds[1]);
            return -1;
        }

        ret = pipe(stderr_fds);
        if (ret < 0) {
            fprintf(stderr, "pipe() failed: %s\n", strerror(errno));
            free_operator(&o);
            close(stdout_fds[1]);
            return -1;
        }

        o->stderr_fd = stderr_fds[0];
        event_set(&o->stderr_event, o->stderr_fd,
                  EV_READ|EV_PERSIST, stderr_callback, (void *)o);
        event_add(&o->stderr_event, NULL);
    }

    /* build command line */

    std::list<std::string> args;
    args.insert(args.end(), plan->args.begin(), plan->args.end());
    args.insert(args.end(), job->args.begin(), job->args.end());

    expand_operator_vars(o, args);

    /* fork */

    o->pid = fork();
    if (o->pid < 0) {
        fprintf(stderr, "fork() failed: %s\n", strerror(errno));
        close(stdout_fds[1]);
        if (!job->syslog_server.empty())
            close(stderr_fds[1]);
        return -1;
    }

    if (o->pid == 0) {
        /* in the operator process */

        clearenv();

        /* swap effective uid back to root */

        if (!debug_mode) {
            ret = setreuid(0, 0);
            if (ret < 0) {
                perror("setreuid() to root failed");
                exit(1);
            }
        }

        /* chroot */

        if (!plan->chroot.empty()) {
            ret = chroot(plan->chroot.c_str());
            fprintf(stderr, "chroot('%s') failed: %s\n",
                    plan->chroot.c_str(), strerror(errno));
            exit(1);
        }

        /* priority */

        ret = setpriority(PRIO_PROCESS, getpid(), plan->priority);
        if (ret < 0) {
            fprintf(stderr, "setpriority() failed: %s\n", strerror(errno));
            exit(1);
        }

        /* UID / GID */

        if (!debug_mode) {
            ret = setgroups(plan->groups.size(), &plan->groups[0]);
            if (ret < 0) {
                fprintf(stderr, "setgroups() failed: %s\n", strerror(errno));
                exit(1);
            }

            ret = setregid(plan->gid, plan->gid);
            if (ret < 0) {
                fprintf(stderr, "setregid() failed: %s\n", strerror(errno));
                exit(1);
            }

            ret = setreuid(plan->uid, plan->uid);
            if (ret < 0) {
                fprintf(stderr, "setreuid() failed: %s\n", strerror(errno));
                exit(1);
            }
        }

        /* connect pipes */

        dup2(stdout_fds[1], 1);
        if (!job->syslog_server.empty())
            dup2(stderr_fds[1], 2);

        close(stdout_fds[0]);
        close(stdout_fds[1]);
        if (!job->syslog_server.empty()) {
            close(stderr_fds[0]);
            close(stderr_fds[1]);
        }

        /* session */

        setsid();

        /* execute plan */

        std::vector<const char *> argv;
        argv.reserve(args.size() + 1);
        for (const auto &a : args)
            argv.push_back(a.c_str());

        argv.push_back(NULL);

        execv(argv[0], const_cast<char *const*>(&argv[0]));
        fprintf(stderr, "execv() failed: %s\n", strerror(errno));
        exit(1);
    }

    close(stdout_fds[1]);
    if (!job->syslog_server.empty())
        close(stderr_fds[1]);

    daemon_log(2, "job %s (plan '%s') running as pid %d\n",
               job->id.c_str(), job->plan_name.c_str(), o->pid);

    operators.push_back(o);
    ++num_operators;

    return 0;
}

Workplace::OperatorList::iterator
Workplace::FindByPid(pid_t pid)
{
    struct ComparePid {
        pid_t pid;

        ComparePid(pid_t _pid):pid(_pid) {}

        bool operator()(const Operator *o) const {
            return o->pid == pid;
        }
    };

    return std::find_if(operators.begin(), operators.end(),
                        ComparePid(pid));
}

void
Workplace::WaitPid()
{
    pid_t pid;
    int status, exit_status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        auto i = FindByPid(pid);
        if (i == operators.end())
            continue;

        Operator *o = *i;

        exit_status = WEXITSTATUS(status);

        if (WIFSIGNALED(status)) {
            daemon_log(1, "job %s (pid %d) died from signal %d%s\n",
                       o->job->id.c_str(), pid,
                       WTERMSIG(status),
                       WCOREDUMP(status) ? " (core dumped)" : "");
            exit_status = -1;
        } else if (exit_status == 0)
            daemon_log(3, "job %s (pid %d) exited with success\n",
                       o->job->id.c_str(), pid);
        else
            daemon_log(2, "job %s (pid %d) exited with status %d\n",
                       o->job->id.c_str(), pid,
                       exit_status);

        plan_put(&o->plan);

        job_done(&o->job, exit_status);

        operators.erase(i);
        --num_operators;
        free_operator(&o);
    }
}
