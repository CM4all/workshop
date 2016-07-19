/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_INSTANCE_HXX
#define WORKSHOP_INSTANCE_HXX

#include "event/Loop.hxx"
#include "event/SignalEvent.hxx"
#include "Library.hxx"
#include "Queue.hxx"
#include "Workplace.hxx"
#include "spawn/Registry.hxx"

class Instance {
public:
    EventLoop event_loop;

    bool should_exit = false;

    SignalEvent sigterm_event, sigint_event, sigquit_event;
    SignalEvent sighup_event;

    ChildProcessRegistry child_process_registry;

    Library library;
    Queue queue;
    Workplace workplace;

    Instance(const char *library_path,
             const char *node_name,
             const char *conninfo, const char *schema,
             unsigned concurrency);

    void UpdateFilter();
    void UpdateLibraryAndFilter();

    void OnChildProcessExit();

private:
    bool StartJob(Job &&job);
    void OnJob(Job &&job);
    void OnExit(int);
    void OnReload(int);
};

#endif
