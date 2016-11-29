/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_INSTANCE_HXX
#define CRON_INSTANCE_HXX

#include "Queue.hxx"
#include "Workplace.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "Workplace.hxx"
#include "spawn/Registry.hxx"
#include "spawn/ExitListener.hxx"

#include <functional>

struct CronConfig;
class SpawnServerClient;

class CronInstance final : ExitListener {
    EventLoop event_loop;

    bool should_exit = false;

    ShutdownListener shutdown_listener;
    SignalEvent sighup_event;

    ChildProcessRegistry child_process_registry;

    std::unique_ptr<SpawnServerClient> spawn_service;

    CronQueue queue;
    CronWorkplace workplace;

public:
    CronInstance(const CronConfig &config,
                 const char *schema,
                 std::function<void()> &&in_spawner);

    ~CronInstance();

    void Start() {
        queue.Connect();
    }

    void Dispatch() {
        event_loop.Dispatch();
    }

private:
    void OnJob(CronJob &&job);

    void OnExit();
    void OnReload(int);

    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;
};

#endif
