/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "Config.hxx"
#include "Job.hxx"
#include "Plan.hxx"
#include "spawn/Client.hxx"
#include "spawn/Glue.hxx"
#include "pg/Array.hxx"
#include "util/PrintException.hxx"

#include <daemon/log.h>

#include <forward_list>

#include <signal.h>

Instance::Instance(const Config &config,
                   const char *schema,
                   std::function<void()> &&in_spawner)
    :shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
     sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
     child_process_registry(event_loop),
     spawn_service(StartSpawnServer(config.spawn, child_process_registry,
                                    [this, &in_spawner](){
                                        in_spawner();
                                        event_loop.Reinit();
                                        event_loop.~EventLoop();
                                    })),
     queue(event_loop, config.node_name, config.database, schema,
           [this](Job &&job){ OnJob(std::move(job)); }),
     workplace(*spawn_service, *this, config.node_name, config.concurrency)
{
    shutdown_listener.Enable();
    sighup_event.Add();
}

Instance::~Instance()
{
}

void
Instance::UpdateFilter()
{
    std::forward_list<std::string> available_plans;
    library.VisitPlans(std::chrono::steady_clock::now(),
                       [&available_plans](const std::string &name, const Plan &){
                           available_plans.emplace_front(name);
                       });

    queue.SetFilter(pg_encode_array(available_plans),
                    workplace.GetFullPlanNames(),
                    workplace.GetRunningPlanNames());
}

void
Instance::UpdateLibraryAndFilter(bool force)
{
    library.Update(force);
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

    shutdown_listener.Disable();
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
    UpdateLibraryAndFilter(true);
}

void
Instance::OnChildProcessExit(int)
{
    if (should_exit) {
        if (workplace.IsEmpty())
            queue.Close();
        return;
    }

    UpdateLibraryAndFilter(false);

    if (!workplace.IsFull())
        queue.Enable();
}

