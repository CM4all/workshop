/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "Config.hxx"
#include "control/Server.hxx"
#include "workshop/MultiLibrary.hxx"
#include "spawn/Client.hxx"
#include "spawn/Glue.hxx"
#include "curl/Global.hxx"

#include <signal.h>

Instance::Instance(const Config &config)
    :shutdown_listener(event_loop, BIND_THIS_METHOD(OnExit)),
     sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
     defer_idle_check(event_loop, BIND_THIS_METHOD(RemoveIdlePartitions)),
     child_process_registry(event_loop),
     curl(new CurlGlobal(event_loop))
{
    /* the plan library must be initialized before starting the
       spawner, because it is required by Verify(), which runs inside
       the spawner process */

    if (!config.partitions.empty()) {
        library.reset(new MultiLibrary());
        library->InsertPath("/etc/cm4all/workshop/plans");
        library->InsertPath("/usr/share/cm4all/workshop/plans");
    }

    auto *ss = StartSpawnServer(config.spawn, child_process_registry,
                                this,
                                [this](){
                                    event_loop.Reinit();
                                    child_process_registry.~ChildProcessRegistry();
                                    event_loop.~EventLoop();
                                });
    spawn_service.reset(ss);

    shutdown_listener.Enable();
    sighup_event.Enable();

    for (const auto &i : config.partitions)
        partitions.emplace_front(*this, *library, *spawn_service,
                                 config, i,
                                 BIND_THIS_METHOD(OnPartitionIdle));

    for (const auto &i : config.cron_partitions)
        cron_partitions.emplace_front(event_loop, *spawn_service, *curl,
                                      config, i,
                                      BIND_THIS_METHOD(OnPartitionIdle));

    ControlHandler &control_handler = *this;
    for (const auto &i : config.control_listen)
        control_servers.emplace_front(event_loop, i.Create(SOCK_DGRAM),
                                      control_handler);
}

Instance::~Instance()
{
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
    sighup_event.Disable();
    child_process_registry.SetVolatile();

    spawn_service->Shutdown();

    for (auto &i : partitions)
        i.BeginShutdown();

    for (auto &i : cron_partitions)
        i.BeginShutdown();

    RemoveIdlePartitions();

    if (!partitions.empty())
        logger(1, "waiting for operators to finish");

    control_servers.clear();
}

void
Instance::OnReload(int)
{
    logger(4, "reloading");

    if (library)
        UpdateLibraryAndFilter(true);
}

void
Instance::OnPartitionIdle()
{
    if (should_exit)
        /* defer the RemoveIdlePartitions() call to avoid deleting the
           partitions while iterating the partition list in
           OnExit() */
        defer_idle_check.Schedule();
}

void
Instance::RemoveIdlePartitions()
{
    partitions.remove_if([](const WorkshopPartition &partition){
            return partition.IsIdle();
        });

    cron_partitions.remove_if([](const CronPartition &partition){
            return partition.IsIdle();
        });
}

void
Instance::OnControlPacket(WorkshopControlCommand command,
                          ConstBuffer<void> payload)
{
    (void)payload;

    switch (command) {
    case WorkshopControlCommand::NOP:
        break;

    case WorkshopControlCommand::VERBOSE:
        {
            const auto *log_level = (const uint8_t *)payload.data;
            if (payload.size != sizeof(*log_level))
                throw std::runtime_error("Malformed VERBOSE packet");

            SetLogLevel(*log_level);
        }
        break;
    }
}

void
Instance::OnControlError(std::exception_ptr ep) noexcept
{
    logger(1, "Control error: ", ep);
}
