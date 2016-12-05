/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "Config.hxx"
#include "workshop/MultiLibrary.hxx"
#include "spawn/Client.hxx"
#include "spawn/Glue.hxx"

#include <daemon/log.h>

#include <signal.h>

Instance::Instance(const Config &config,
                   std::function<void()> &&in_spawner)
    :shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
     sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
     child_process_registry(event_loop),
     spawn_service(StartSpawnServer(config.spawn, child_process_registry,
                                    [this, &in_spawner](){
                                        in_spawner();
                                        event_loop.Reinit();
                                        event_loop.~EventLoop();
                                    }))
{
    shutdown_listener.Enable();
    sighup_event.Add();

    if (!config.partitions.empty())
        library.reset(new MultiLibrary());

    for (const auto &i : config.partitions)
        partitions.emplace_front(*this, *library, *spawn_service,
                                 config, i,
                                 BIND_THIS_METHOD(OnPartitionIdle));
}

Instance::~Instance()
{
}

void
Instance::InsertLibraryPath(const char *path)
{
    if (library)
        library->InsertPath(path);
}

void
Instance::UpdateFilter()
{
    for (auto &i : partitions)
        i.UpdateFilter();
}

void
Instance::UpdateLibraryAndFilter(bool force)
{
    assert(library);

    library->Update(force);
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

    spawn_service->Shutdown();

    for (auto &i : partitions)
        i.BeginShutdown();

    RemoveIdlePartitions();

    if (!partitions.empty())
        daemon_log(1, "waiting for operators to finish\n");
}

void
Instance::OnReload(int)
{
    daemon_log(4, "reloading\n");

    if (library)
        UpdateLibraryAndFilter(true);
}

void
Instance::OnPartitionIdle()
{
    if (should_exit)
        RemoveIdlePartitions();
}

void
Instance::RemoveIdlePartitions()
{
    partitions.remove_if([](const WorkshopPartition &partition){
            return partition.IsIdle();
        });
}
