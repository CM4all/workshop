/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_INSTANCE_HXX
#define WORKSHOP_INSTANCE_HXX

#include "event/Loop.hxx"
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
public:
    EventLoop event_loop;

    bool should_exit = false;

    SignalEvent sigterm_event, sigint_event, sigquit_event;
    SignalEvent sighup_event;

    ChildProcessRegistry child_process_registry;

    std::unique_ptr<SpawnServerClient> spawn_service;

    MultiLibrary library;
    Queue queue;
    Workplace workplace;

    Instance(const Config &config,
             const char *schema,
             std::function<void()> &&in_spawner);

    ~Instance();

    void Start() {
        queue.Connect();
    }

    void UpdateFilter();
    void UpdateLibraryAndFilter(bool force);

private:
    bool StartJob(Job &&job);
    void OnJob(Job &&job);
    void OnExit(int);
    void OnReload(int);

    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;
};

#endif
