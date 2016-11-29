/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_INSTANCE_HXX
#define CRON_INSTANCE_HXX

#include "Partition.hxx"
#include "Workplace.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
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

    CronWorkplace workplace;

    std::forward_list<CronPartition> partitions;

public:
    CronInstance(const CronConfig &config,
                 std::function<void()> &&in_spawner);

    ~CronInstance();

    EventLoop &GetEventLoop() {
        return event_loop;
    }

    CronWorkplace &GetWorkplace() {
        return workplace;
    }

    void Start() {
        for (auto &i : partitions)
            i.Start();
    }

    void DisableAllQueues() {
        for (auto &i : partitions)
            i.Disable();
    }

    void Dispatch() {
        event_loop.Dispatch();
    }

private:
    void OnExit();
    void OnReload(int);

    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;
};

#endif
