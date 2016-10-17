/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_CONFIG_HXX
#define CRON_CONFIG_HXX

#include "spawn/Config.hxx"

#include <daemon/user.h>

struct CronConfig {
    struct daemon_user user;

    const char *node_name = nullptr;
    unsigned concurrency = 8;
    const char *database = nullptr;

    const char *translation_socket = "@translation";

    SpawnConfig spawn;

    CronConfig();
};

#endif
