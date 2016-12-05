/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Partition.hxx"
#include "Instance.hxx"
#include "Job.hxx"
#include "Plan.hxx"
#include "../Config.hxx"
#include "pg/Array.hxx"
#include "util/PrintException.hxx"

#include <set>

WorkshopPartition::WorkshopPartition(Instance &_instance,
                                     MultiLibrary &_library,
                                     SpawnService &_spawn_service,
                                     const Config &root_config,
                                     const WorkshopPartitionConfig &config,
                                     BoundMethod<void()> _idle_callback)
    :instance(_instance), library(_library),
     queue(instance.GetEventLoop(), root_config.node_name.c_str(),
           config.database.c_str(), config.database_schema.c_str(),
           [this](WorkshopJob &&job){ OnJob(std::move(job)); }),
     workplace(_spawn_service, *this,
               root_config.node_name.c_str(),
               root_config.concurrency),
     idle_callback(_idle_callback)
{
}

void
WorkshopPartition::UpdateFilter()
{
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
WorkshopPartition::UpdateLibraryAndFilter(bool force)
{
    instance.UpdateLibraryAndFilter(force);
}

bool
WorkshopPartition::StartJob(WorkshopJob &&job)
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
        workplace.Start(instance.GetEventLoop(), job, std::move(plan));
    } catch (const std::runtime_error &e) {
        PrintException(e);

        queue.SetJobDone(job, -1);
    }

    return true;
}

void
WorkshopPartition::OnJob(WorkshopJob &&job)
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
WorkshopPartition::OnChildProcessExit(int)
{
    UpdateLibraryAndFilter(false);

    if (!workplace.IsFull())
        queue.Enable();

    if (IsIdle())
        idle_callback();
}

