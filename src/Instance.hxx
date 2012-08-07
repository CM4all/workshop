/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_INSTANCE_HXX
#define WORKSHOP_INSTANCE_HXX

#include <event.h>

class Library;
struct Queue;
class Workplace;

class Instance {
public:
    Library *library = nullptr;
    Queue *queue = nullptr;
    Workplace *workplace = nullptr;
    bool should_exit = false;
    struct event sigterm_event, sigint_event, sigquit_event;
    struct event sighup_event, sigchld_event;
};

#endif
