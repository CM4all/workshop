/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_CONFIG_HXX
#define WORKSHOP_CONFIG_HXX

#include "spawn/Config.hxx"

#include <daemon/user.h>

#include <string>
#include <forward_list>

struct Config {
    struct daemon_user user;

    std::string node_name;
    unsigned concurrency = 2;

    SpawnConfig spawn;

    struct Partition {
        std::string database, database_schema;

        Partition() = default;
        explicit Partition(const char *_database):database(_database) {}

        void Check() const;
    };

    std::forward_list<Partition> partitions;

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
