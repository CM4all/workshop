/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_INSTANCE_HXX
#define WORKSHOP_INSTANCE_HXX

#include "workshop/Partition.hxx"
#include "workshop/MultiLibrary.hxx"
#include "cron/Partition.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "event/DeferEvent.hxx"
#include "spawn/Registry.hxx"
#include "spawn/Hook.hxx"
#include "curl/Init.hxx"
#include "io/Logger.hxx"

#include <forward_list>

struct Config;
class SpawnServerClient;
class CurlGlobal;
class MultiLibrary;

class Instance final : SpawnHook {
    const RootLogger logger;

    EventLoop event_loop;

    bool should_exit = false;

    ShutdownListener shutdown_listener;
    SignalEvent sighup_event;
    DeferEvent defer_idle_check;

    ChildProcessRegistry child_process_registry;

    std::unique_ptr<SpawnServerClient> spawn_service;

    ScopeCurlInit curl_init;
    std::unique_ptr<CurlGlobal> curl;

    std::unique_ptr<MultiLibrary> library;

    std::forward_list<WorkshopPartition> partitions;

    std::forward_list<CronPartition> cron_partitions;

public:
    explicit Instance(const Config &config);

    ~Instance();

    EventLoop &GetEventLoop() {
        return event_loop;
    }

    CurlGlobal &GetCurl() {
        return *curl;
    }

    void Start() {
        for (auto &i : partitions)
            i.Start();
        for (auto &i : cron_partitions)
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

    /* virtual methods from SpawnHook */
    bool Verify(const PreparedChildProcess &p) override;
};

#endif
