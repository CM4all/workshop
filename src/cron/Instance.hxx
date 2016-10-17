/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_INSTANCE_HXX
#define CRON_INSTANCE_HXX

#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "Workplace.hxx"
#include "spawn/Registry.hxx"

#include <functional>

struct CronConfig;
class SpawnServerClient;

class CronInstance final {
public:
    EventLoop event_loop;

    bool should_exit = false;

    ShutdownListener shutdown_listener;
    SignalEvent sighup_event;

    ChildProcessRegistry child_process_registry;

    std::unique_ptr<SpawnServerClient> spawn_service;

    CronInstance(const CronConfig &config,
                 const char *schema,
                 std::function<void()> &&in_spawner);

    ~CronInstance();

    void Start() {
        // TODO: db.Connect()
    }

private:
    void OnExit();
    void OnReload(int);
};

#endif
