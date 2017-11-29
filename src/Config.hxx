/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef _WORKSHOP_CONFIG_HXX
#define _WORKSHOP_CONFIG_HXX

#include "workshop/Config.hxx"
#include "cron/Config.hxx"
#include "spawn/Config.hxx"
#include "net/SocketConfig.hxx"

#include <string>
#include <forward_list>

struct Config {
    UidGid user;

    std::string node_name;
    unsigned concurrency = 2;

    SpawnConfig spawn;

    std::forward_list<WorkshopPartitionConfig> partitions;
    std::forward_list<CronPartitionConfig> cron_partitions;

    struct ControlListener : SocketConfig {
        ControlListener() {
            pass_cred = true;
        }

        explicit ControlListener(SocketAddress _bind_address)
            :SocketConfig(_bind_address) {
            pass_cred = true;
        }
    };

    std::forward_list<ControlListener> control_listen;

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
