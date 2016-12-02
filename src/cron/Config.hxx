/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_CONFIG_HXX
#define CRON_CONFIG_HXX

#include "spawn/Config.hxx"

#include <daemon/user.h>

#include <forward_list>

struct CronPartitionConfig {
    std::string database, database_schema;

    std::string translation_socket;

    void Check() const;
};

struct CronConfig {
    struct daemon_user user;

    std::string node_name;
    unsigned concurrency = 8;

    SpawnConfig spawn;

    std::forward_list<CronPartitionConfig> partitions;

    CronConfig();

    void Check();
};

/**
 * Load and parse the specified configuration file.  Throws an
 * exception on error.
 */
void
LoadConfigFile(CronConfig &config, const char *path);

#endif
