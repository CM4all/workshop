/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_INSTANCE_HXX
#define WORKSHOP_INSTANCE_HXX

#include "workshop/Partition.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "spawn/Registry.hxx"

#include <functional>
#include <forward_list>

struct Config;
class SpawnServerClient;
class MultiLibrary;

class Instance final {
    EventLoop event_loop;

    bool should_exit = false;

    ShutdownListener shutdown_listener;
    SignalEvent sighup_event;

    ChildProcessRegistry child_process_registry;

    std::unique_ptr<SpawnServerClient> spawn_service;

    std::unique_ptr<MultiLibrary> library;

    std::forward_list<WorkshopPartition> partitions;

public:
    Instance(const Config &config,
             std::function<void()> &&in_spawner);

    ~Instance();

    EventLoop &GetEventLoop() {
        return event_loop;
    }

    void InsertLibraryPath(const char *path);

    void Start() {
        for (auto &i : partitions)
            i.Start();
    }

    void Dispatch() {
        event_loop.Dispatch();
    }

    void UpdateFilter();
    void UpdateLibraryAndFilter(bool force);

private:
    void OnExit();
    void OnReload(int);

    void OnPartitionIdle();
    void RemoveIdlePartitions();
};

#endif
