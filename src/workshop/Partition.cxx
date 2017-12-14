/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Partition.hxx"
#include "Instance.hxx"
#include "MultiLibrary.hxx"
#include "Config.hxx"
#include "Job.hxx"
#include "Plan.hxx"
#include "../Config.hxx"
#include "pg/Array.hxx"
#include "util/Exception.hxx"

#include <set>

WorkshopPartition::WorkshopPartition(Instance &_instance,
                                     MultiLibrary &_library,
                                     SpawnService &_spawn_service,
                                     const Config &root_config,
                                     const WorkshopPartitionConfig &config,
                                     BoundMethod<void()> _idle_callback)
    :logger("workshop"), // TODO: add partition name
     instance(_instance), library(_library),
     queue(logger, instance.GetEventLoop(), root_config.node_name.c_str(),
           config.database.c_str(), config.database_schema.c_str(),
           [this](WorkshopJob &&job){ OnJob(std::move(job)); }),
     workplace(_spawn_service, *this, logger,
               root_config.node_name.c_str(),
               root_config.concurrency,
               config.enable_journal),
     idle_callback(_idle_callback),
     max_log(config.max_log)
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

    queue.SetFilter(Pg::EncodeArray(available_plans),
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
        logger(1, "library_get('", job.plan_name, "') failed");
        queue.RollbackJob(job);
        return false;
    }

    int ret = job.SetProgress(0, plan->timeout.c_str());
    if (ret < 0) {
        queue.RollbackJob(job);
        return false;
    }

    try {
        workplace.Start(instance.GetEventLoop(), job, std::move(plan),
                        max_log);
    } catch (...) {
        logger(1, "failed to start job '", job.id,
               "' plan '", job.plan_name, "': ", std::current_exception());

        queue.SetJobDone(job, -1, nullptr);
    }

    return true;
}

void
WorkshopPartition::OnJob(WorkshopJob &&job)
{
    if (workplace.IsFull()) {
        queue.RollbackJob(job);
        queue.DisableFull();
        return;
    }

    if (!StartJob(std::move(job)) || workplace.IsFull())
        queue.DisableFull();

    UpdateFilter();
}

void
WorkshopPartition::OnChildProcessExit(int)
{
    UpdateLibraryAndFilter(false);

    if (!workplace.IsFull())
        queue.EnableFull();

    if (IsIdle())
        idle_callback();
}

