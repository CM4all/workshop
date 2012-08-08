/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "Job.hxx"

#include <daemon/log.h>

#include <signal.h>

Instance::Instance(const char *library_path,
                   const char *node_name, const char *conninfo,
                   unsigned concurrency)
    :sigterm_event(SIGTERM, [this](){ OnExit(); }),
     sigint_event(SIGINT, [this](){ OnExit(); }),
     sigquit_event(SIGQUIT, [this](){ OnExit(); }),
     sighup_event(SIGHUP, [this](){ OnReload(); }),
     sigchld_event(SIGCHLD, [this](){ OnChild(); }),
     library(library_path),
     queue(node_name, conninfo,
           [this](Job &&job){ OnJob(std::move(job)); }),
     workplace(node_name, concurrency)
{
}

void
Instance::UpdateFilter()
{
    queue.SetFilter(library.GetPlanNames(),
                    workplace.GetFullPlanNames(),
                    workplace.GetRunningPlanNames());
}

void
Instance::UpdateLibraryAndFilter()
{
    library.Update();
    UpdateFilter();
}

bool
Instance::StartJob(Job &&job)
{
    Plan *plan = library.Get(job.plan_name.c_str());
    if (plan == nullptr) {
        fprintf(stderr, "library_get('%s') failed\n", job.plan_name.c_str());
        queue.RollbackJob(job);
        return false;
    }

    int ret = job.SetProgress(0, plan->timeout.c_str());
    if (ret < 0) {
        queue.RollbackJob(job);
        return false;
    }

    ret = workplace.Start(job, plan);
    if (ret != 0) {
        plan_put(&plan);
        queue.SetJobDone(job, -1);
    }

    return true;
}

void
Instance::OnJob(Job &&job)
{
    if (workplace.IsFull()) {
        queue.RollbackJob(job);
        queue.Disable();
        return;
    }

    library.Update();

    if (!StartJob(std::move(job)) || workplace.IsFull())
        queue.Disable();

    UpdateFilter();
}

void
Instance::OnExit()
{
    if (should_exit)
        return;

    should_exit = true;

    sigterm_event.Delete();
    sigint_event.Delete();
    sigquit_event.Delete();
    sighup_event.Delete();
    sigchld_event.Delete();

    queue.Disable();

    if (workplace.IsEmpty())
        queue.Close();
    else
        daemon_log(1, "waiting for operators to finish\n");
}

void
Instance::OnReload()
{
    daemon_log(4, "reloading\n");
    UpdateLibraryAndFilter();
    queue.Reschedule();
}

void
Instance::OnChild()
{
    workplace.WaitPid();

    if (should_exit) {
        if (workplace.IsEmpty())
            queue.Close();
        return;
    }

    UpdateLibraryAndFilter();

    if (!workplace.IsFull())
        queue.Enable();
}

