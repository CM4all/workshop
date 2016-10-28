/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "Config.hxx"
#include "Job.hxx"
#include "spawn/Client.hxx"
#include "spawn/Glue.hxx"

#include <daemon/log.h>

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
                                    })),
     queue(event_loop, config.node_name, config.database, schema,
           [this](CronJob &&job){ OnJob(std::move(job)); })
{
    shutdown_listener.Enable();
    sighup_event.Add();
}

CronInstance::~CronInstance()
{
}

void
CronInstance::OnJob(CronJob &&job)
{
    printf("OnJob '%s'\n", job.id.c_str());

    if (!queue.Claim(job))
        return;

    // TODO: execute
    queue.Finish(job);
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

    queue.Close();
}

void
CronInstance::OnReload(int)
{
    daemon_log(4, "reloading\n");
    // TODO
}
