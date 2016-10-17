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

#include <set>

#include <signal.h>

CronInstance::CronInstance(const CronConfig &config,
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
                                    }))
{
    shutdown_listener.Enable();
    sighup_event.Add();

    (void)schema; // TODO
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
}

void
CronInstance::OnReload(int)
{
    daemon_log(4, "reloading\n");
    // TODO
}
