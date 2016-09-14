/*
 * Manage operator processes.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Workplace.hxx"
#include "Operator.hxx"
#include "debug.h"
#include "SyslogClient.hxx"
#include "Plan.hxx"
#include "Job.hxx"
#include "pg/Array.hxx"
#include "spawn/Direct.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/Config.hxx"
#include "spawn/CgroupState.hxx"

#include <inline/compiler.h>
#include <daemon/log.h>

#include <algorithm>
#include <string>
#include <map>
#include <list>
#include <vector>

#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
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

std::string
Workplace::GetRunningPlanNames() const
{
    std::list<std::string> list;
    for (const auto &o : operators)
        if (!contains(list, o->job.plan_name))
            list.push_back(o->job.plan_name);

    return pg_encode_array(list);
}

std::string
Workplace::GetFullPlanNames() const
{
    std::map<std::string, unsigned> counters;
    std::list<std::string> list;
    for (const auto &o : operators) {
        const Plan &plan = *o->plan;
        if (plan.concurrency == 0)
            continue;

        const std::string &plan_name = o->job.plan_name;

        auto i = counters.emplace(plan_name, 0);
        unsigned &n = i.first->second;

        ++n;

        assert(n <= plan.concurrency || contains(list, plan_name));

        if (n == plan.concurrency)
            list.push_back(plan_name);
    }

    return pg_encode_array(list);
}

int
Workplace::Start(EventLoop &event_loop, const Job &job,
                 std::shared_ptr<Plan> &&plan)
{
    assert(!plan->args.empty());

    /* create operator object */

    std::unique_ptr<Operator> o(new Operator(event_loop, *this, job, plan));

    PreparedChildProcess p;

    if (!debug_mode) {
        p.uid_gid.uid = plan->uid;
        p.uid_gid.gid = plan->gid;

        std::copy(plan->groups.begin(), plan->groups.end(),
                  p.uid_gid.groups.begin());

        p.regain_root = true;
    }

    if (!plan->chroot.empty())
        p.chroot = plan->chroot.c_str();

    p.priority = plan->priority;

    /* create stdout/stderr pipes */

    {
        UniqueFileDescriptor stdout_r, stdout_w;
        if (!UniqueFileDescriptor::CreatePipe(stdout_r, stdout_w)) {
            fprintf(stderr, "pipe() failed: %s\n", strerror(errno));
            return -1;
        }

        o->SetOutput(std::move(stdout_r));
        p.SetStdout(std::move(stdout_w));
    }

    if (!job.syslog_server.empty()) {
        char ident[256];

        snprintf(ident, sizeof(ident), "%s[%s]",
                 job.plan_name.c_str(), job.id.c_str());

        try {
            o->syslog.reset(SyslogClient::Create(node_name.c_str(), ident, 1,
                                                 job.syslog_server.c_str()));
        } catch (const std::runtime_error &e) {
            fprintf(stderr, "syslog_open(%s) failed: %s\n",
                    job.syslog_server.c_str(), e.what());
            return -1;
        }

        UniqueFileDescriptor stderr_r, stderr_w;
        if (!UniqueFileDescriptor::CreatePipe(stderr_r, stderr_w)) {
            fprintf(stderr, "pipe() failed: %s\n", strerror(errno));
            return -1;
        }

        o->SetSyslog(std::move(stderr_r));
        p.SetStderr(std::move(stderr_w));
    }

    /* build command line */

    std::list<std::string> args;
    args.insert(args.end(), plan->args.begin(), plan->args.end());
    args.insert(args.end(), job.args.begin(), job.args.end());

    o->Expand(args);

    for (const auto &i : args)
        p.args.push_back(i.c_str());

    /* fork */

    o->pid = SpawnChildProcess(std::move(p), SpawnConfig(), CgroupState());

    if (o->pid < 0) {
        fprintf(stderr, "fork() failed: %s\n", strerror(-o->pid));
        return -1;
    }

    daemon_log(2, "job %s (plan '%s') running as pid %d\n",
               job.id.c_str(), job.plan_name.c_str(), o->pid);

    operators.push_back(o.release());
    ++num_operators;

    return 0;
}

Workplace::OperatorList::iterator
Workplace::FindByPid(pid_t pid)
{
    return std::find_if(operators.begin(), operators.end(),
                        [pid](const Operator *o) { return o->pid == pid; });
}

void
Workplace::WaitPid()
{
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        auto i = FindByPid(pid);
        if (i == operators.end())
            continue;

        Operator *o = *i;
        o->OnProcessExit(status);
        operators.erase(i);
        --num_operators;
        delete o;
    }
}
