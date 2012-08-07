/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_INSTANCE_HXX
#define WORKSHOP_INSTANCE_HXX

#include "Event.hxx"

class Library;
struct Queue;
class Workplace;

class Instance {
public:
    Library *library = nullptr;
    Queue *queue = nullptr;
    Workplace *workplace = nullptr;
    bool should_exit = false;

    SignalEvent sigterm_event, sigint_event, sigquit_event;
    SignalEvent sighup_event, sigchld_event;

    Instance();

    void UpdateFilter();
    void UpdateLibraryAndFilter();

private:
    void OnExit();
    void OnReload();
    void OnChild();
};

#endif
