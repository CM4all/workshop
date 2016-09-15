/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_CONFIG_HXX
#define WORKSHOP_CONFIG_HXX

#include "spawn/Config.hxx"

#include <daemon/user.h>

struct Config {
    struct daemon_user user;

    const char *node_name = nullptr;
    unsigned concurrency = 2;
    const char *database = nullptr;

    SpawnConfig spawn;

    Config();
};

#endif
