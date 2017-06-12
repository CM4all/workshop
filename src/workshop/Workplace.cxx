/*
 * Manage operator processes.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Workplace.hxx"
#include "debug.h"
#include "Plan.hxx"
#include "Job.hxx"
#include "pg/Array.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/Interface.hxx"
#include "system/Error.hxx"
#include "util/RuntimeError.hxx"

#include <inline/compiler.h>
#include <daemon/log.h>

#include <string>
#include <map>
#include <set>
#include <list>

#include <assert.h>

std::string
WorkshopWorkplace::GetRunningPlanNames() const
{
    std::set<std::string> list;
    for (const auto &o : operators)
        list.emplace(o.GetPlanName());

    return Pg::EncodeArray(list);
}

std::string
WorkshopWorkplace::GetFullPlanNames() const
{
    std::map<std::string, unsigned> counters;
    std::set<std::string> list;
    for (const auto &o : operators) {
        const Plan &plan = o.GetPlan();
        if (plan.concurrency == 0)
            continue;

        const std::string &plan_name = o.GetPlanName();

        auto i = counters.emplace(plan_name, 0);
        unsigned &n = i.first->second;

        ++n;

        assert(n <= plan.concurrency || list.find(plan_name) != list.end());

        if (n == plan.concurrency)
            list.emplace(plan_name);
    }

    return Pg::EncodeArray(list);
}

void
WorkshopWorkplace::Start(EventLoop &event_loop, const WorkshopJob &job,
                         std::shared_ptr<Plan> &&plan,
                         size_t max_log)
{
    assert(!plan->args.empty());

    /* create stdout/stderr pipes */

    UniqueFileDescriptor stderr_r, stderr_w;
    if (!UniqueFileDescriptor::CreatePipe(stderr_r, stderr_w))
        throw MakeErrno("pipe() failed");

    /* create operator object */

    auto o = std::make_unique<WorkshopOperator>(event_loop, *this, job, plan,
                                                std::move(stderr_r),
                                                max_log,
                                                enable_journal);

    PreparedChildProcess p;
    p.hook_info = job.plan_name.c_str();
    p.SetStderr(std::move(stderr_w));

    if (!debug_mode) {
        p.uid_gid.uid = plan->uid;
        p.uid_gid.gid = plan->gid;

        std::copy(plan->groups.begin(), plan->groups.end(),
                  p.uid_gid.groups.begin());
    }

    if (!plan->chroot.empty())
        p.chroot = plan->chroot.c_str();

    p.priority = plan->priority;

    /* create stdout/stderr pipes */

    {
        UniqueFileDescriptor stdout_r, stdout_w;
        if (!UniqueFileDescriptor::CreatePipe(stdout_r, stdout_w))
            throw MakeErrno("pipe() failed");

        o->SetOutput(std::move(stdout_r));
        p.SetStdout(std::move(stdout_w));
    }

    if (!job.syslog_server.empty()) {
        o->CreateSyslogClient(node_name.c_str(), 1,
                              job.syslog_server.c_str());
    }

    /* build command line */

    std::list<std::string> args;
    args.insert(args.end(), plan->args.begin(), plan->args.end());
    args.insert(args.end(), job.args.begin(), job.args.end());

    o->Expand(args);

    for (const auto &i : args)
        p.args.push_back(i.c_str());

    /* fork */

    const auto pid = spawn_service.SpawnChildProcess(job.id.c_str(),
                                                     std::move(p),
                                                     o.get());
    o->SetPid(pid);

    daemon_log(2, "job %s (plan '%s') running as pid %d\n",
               job.id.c_str(), job.plan_name.c_str(), pid);

    operators.push_back(*o.release());
}

void
WorkshopWorkplace::OnExit(WorkshopOperator *o)
{
    operators.erase(operators.iterator_to(*o));
    delete o;

    exit_listener.OnChildProcessExit(-1);
}
