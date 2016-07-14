/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "Job.hxx"

#include <daemon/log.h>

#include <signal.h>

Instance::Instance(const char *library_path,
                   const char *node_name,
                   const char *conninfo, const char *schema,
                   unsigned concurrency)
    :sigterm_event(event_loop, SIGTERM, BIND_THIS_METHOD(OnExit)),
     sigint_event(event_loop, SIGINT, BIND_THIS_METHOD(OnExit)),
     sigquit_event(event_loop, SIGQUIT, BIND_THIS_METHOD(OnExit)),
     sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
     sigchld_event(event_loop, SIGCHLD, BIND_THIS_METHOD(OnChild)),
     library(library_path),
     queue(node_name, conninfo, schema,
           [this](Job &&job){ OnJob(std::move(job)); }),
     workplace(node_name, concurrency)
{
    sigterm_event.Add();
    sigint_event.Add();
    sigquit_event.Add();
    sighup_event.Add();
    sigchld_event.Add();
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
Instance::OnExit(int)
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
Instance::OnReload(int)
{
    daemon_log(4, "reloading\n");
    UpdateLibraryAndFilter();
    queue.Reschedule();
}

void
Instance::OnChild(int)
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
