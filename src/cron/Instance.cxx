/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "Config.hxx"
#include "spawn/Client.hxx"
#include "spawn/Glue.hxx"

#include <daemon/log.h>

#include <signal.h>

CronInstance::CronInstance(const CronConfig &config,
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
     workplace(*spawn_service, *this,
               config.concurrency)
{
    shutdown_listener.Enable();
    sighup_event.Add();

    for (const auto &i : config.partitions)
        partitions.emplace_front(*this, config, i);
}

CronInstance::~CronInstance()
{
}

void
CronInstance::OnExit()
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

    if (workplace.IsEmpty()) {
        partitions.clear();
    } else
        daemon_log(1, "waiting for operators to finish\n");
}

void
CronInstance::OnReload(int)
{
    daemon_log(4, "reloading\n");
    // TODO
}

void
CronInstance::OnChildProcessExit(int)
{
    if (should_exit) {
        if (workplace.IsEmpty()) {
            partitions.clear();
        }

        return;
    }

    if (!workplace.IsFull()) {
        for (auto &i : partitions)
            i.Enable();
    }
}
