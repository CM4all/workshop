/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef _WORKSHOP_CONFIG_HXX
#define _WORKSHOP_CONFIG_HXX

#include "workshop/Config.hxx"
#include "spawn/Config.hxx"

#include <daemon/user.h>

#include <string>
#include <forward_list>

struct Config {
    struct daemon_user user;

    std::string node_name;
    unsigned concurrency = 2;

    SpawnConfig spawn;

    std::forward_list<WorkshopPartitionConfig> partitions;

    Config();

    void Check();
};

/**
 * Load and parse the specified configuration file.  Throws an
 * exception on error.
 */
void
LoadConfigFile(Config &config, const char *path);

#endif
