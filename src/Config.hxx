/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_CONFIG_HXX
#define WORKSHOP_CONFIG_HXX

#include "spawn/Config.hxx"

#include <daemon/user.h>

#include <string>

struct Config {
    struct daemon_user user;

    std::string node_name;
    unsigned concurrency = 2;
    std::string database, database_schema;

    SpawnConfig spawn;

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
