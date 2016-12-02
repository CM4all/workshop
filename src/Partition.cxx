/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Partition.hxx"
#include "Instance.hxx"
#include "Config.hxx"
#include "Job.hxx"
#include "Plan.hxx"
#include "pg/Array.hxx"
#include "util/PrintException.hxx"

#include <set>

Partition::Partition(Instance &_instance, SpawnService &_spawn_service,
                     const Config &config,
                     BoundMethod<void()> _idle_callback)
    :instance(_instance),
     queue(instance.GetEventLoop(), config.node_name.c_str(),
           config.database.c_str(), config.database_schema.c_str(),
           [this](Job &&job){ OnJob(std::move(job)); }),
     workplace(_spawn_service, *this,
               config.node_name.c_str(),
               config.concurrency),
     idle_callback(_idle_callback)
{
}

void
Partition::UpdateFilter()
{
    auto &library = instance.GetLibrary();

    std::set<std::string> available_plans;
    library.VisitPlans(std::chrono::steady_clock::now(),
                       [&available_plans](const std::string &name, const Plan &){
                           available_plans.emplace(name);
                       });

    queue.SetFilter(pg_encode_array(available_plans),
                    workplace.GetFullPlanNames(),
                    workplace.GetRunningPlanNames());
}

void
Partition::UpdateLibraryAndFilter(bool force)
{
    instance.UpdateLibraryAndFilter(force);
}

bool
Partition::StartJob(Job &&job)
{
    auto &library = instance.GetLibrary();
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
        workplace.Start(instance.GetEventLoop(), job, std::move(plan));
    } catch (const std::runtime_error &e) {
        PrintException(e);

        queue.SetJobDone(job, -1);
    }

    return true;
}

void
Partition::OnJob(Job &&job)
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
Partition::OnChildProcessExit(int)
{
    UpdateLibraryAndFilter(false);

    if (!workplace.IsFull())
        queue.Enable();

    if (IsIdle())
        idle_callback();
}

