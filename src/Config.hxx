/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef _WORKSHOP_CONFIG_HXX
#define _WORKSHOP_CONFIG_HXX

#include "workshop/Config.hxx"
#include "cron/Config.hxx"
#include "spawn/Config.hxx"

#include <string>
#include <forward_list>

struct Config {
    UidGid user;

    std::string node_name;
    unsigned concurrency = 2;

    SpawnConfig spawn;

    std::forward_list<WorkshopPartitionConfig> partitions;
    std::forward_list<CronPartitionConfig> cron_partitions;

    void Check();
};

/**
 * Load and parse the specified configuration file.  Throws an
 * exception on error.
 */
void
LoadConfigFile(Config &config, const char *path);

#endif
