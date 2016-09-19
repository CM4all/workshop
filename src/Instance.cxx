/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "Config.hxx"
#include "Job.hxx"
#include "Plan.hxx"
#include "spawn/Client.hxx"
#include "spawn/Glue.hxx"
#include "util/PrintException.hxx"

#include <daemon/log.h>

#include <signal.h>

Instance::Instance(const char *library_path,
                   const Config &config,
                   const char *schema,
                   std::function<void()> &&in_spawner)
    :sigterm_event(event_loop, SIGTERM, BIND_THIS_METHOD(OnExit)),
     sigint_event(event_loop, SIGINT, BIND_THIS_METHOD(OnExit)),
     sigquit_event(event_loop, SIGQUIT, BIND_THIS_METHOD(OnExit)),
     sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
     child_process_registry(event_loop),
     spawn_service(StartSpawnServer(config.spawn, child_process_registry,
                                    [this, &in_spawner](){
                                        in_spawner();
                                        event_loop.Reinit();
                                        event_loop.~EventLoop();
                                    })),
     library(library_path),
     queue(event_loop, config.node_name, config.database, schema,
           [this](Job &&job){ OnJob(std::move(job)); }),
     workplace(*spawn_service, *this, config.node_name, config.concurrency)
{
    sigterm_event.Add();
    sigint_event.Add();
    sigquit_event.Add();
    sighup_event.Add();
}

Instance::~Instance()
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
    auto plan = library.Get(job.plan_name.c_str());
    if (!plan) {
        fprintf(stderr, "library_get('%s') failed\n", job.plan_name.c_str());
        queue.RollbackJob(job);
        return false;
    }

    int ret = job.SetProgress(0, plan->timeout.c_str());
    if (ret < 0) {
        queue.RollbackJob(job);
        return false;
    }

    try {
        workplace.Start(event_loop, job, std::move(plan));
    } catch (const std::runtime_error &e) {
        PrintException(e);

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
    child_process_registry.SetVolatile();

    queue.Disable();

    spawn_service->Shutdown();

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
Instance::OnChildProcessExit(int)
{
    if (should_exit) {
        if (workplace.IsEmpty())
            queue.Close();
        return;
    }

    UpdateLibraryAndFilter();

    if (!workplace.IsFull())
        queue.Enable();
}

