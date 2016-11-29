/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_CONFIG_HXX
#define CRON_CONFIG_HXX

#include "spawn/Config.hxx"

#include <daemon/user.h>

#include <forward_list>

struct CronConfig {
    struct daemon_user user;

    std::string node_name;
    unsigned concurrency = 8;

    SpawnConfig spawn;

    struct Partition {
        std::string database;

        std::string translation_socket;

        void Check() const;
    };

    std::forward_list<Partition> partitions;

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
