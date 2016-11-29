/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_INSTANCE_HXX
#define WORKSHOP_INSTANCE_HXX

#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "MultiLibrary.hxx"
#include "Queue.hxx"
#include "Workplace.hxx"
#include "spawn/Registry.hxx"
#include "spawn/ExitListener.hxx"

#include <functional>

struct Config;
class SpawnServerClient;

class Instance final : ExitListener {
    EventLoop event_loop;

    bool should_exit = false;

    ShutdownListener shutdown_listener;
    SignalEvent sighup_event;

    ChildProcessRegistry child_process_registry;

    std::unique_ptr<SpawnServerClient> spawn_service;

    MultiLibrary library;
    Queue queue;
    Workplace workplace;

public:
    Instance(const Config &config,
             std::function<void()> &&in_spawner);

    ~Instance();

    void InsertLibraryPath(const char *path) {
        library.InsertPath(path);
    }

    void Start() {
        queue.Connect();
    }

    void Dispatch() {
        event_loop.Dispatch();
    }

    void UpdateFilter();
    void UpdateLibraryAndFilter(bool force);

private:
    bool StartJob(Job &&job);
    void OnJob(Job &&job);
    void OnExit();
    void OnReload(int);

    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;
};

#endif
