/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_INSTANCE_HXX
#define CRON_INSTANCE_HXX

#include "Partition.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "spawn/Registry.hxx"

#include <functional>
#include <forward_list>

struct CronConfig;
class SpawnServerClient;

class CronInstance final {
    EventLoop event_loop;

    bool should_exit = false;

    ShutdownListener shutdown_listener;
    SignalEvent sighup_event;

    ChildProcessRegistry child_process_registry;

    std::unique_ptr<SpawnServerClient> spawn_service;

    std::forward_list<CronPartition> partitions;

public:
    CronInstance(const CronConfig &config,
                 std::function<void()> &&in_spawner);

    ~CronInstance();

    EventLoop &GetEventLoop() {
        return event_loop;
    }

    void Start() {
        for (auto &i : partitions)
            i.Start();
    }

    void Dispatch() {
        event_loop.Dispatch();
    }

private:
    void OnExit();
    void OnReload(int);

    void OnPartitionIdle();
    void RemoveIdlePartitions();
};

#endif
