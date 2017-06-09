/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_CONFIG_HXX
#define CRON_CONFIG_HXX

#include "net/AllocatedSocketAddress.hxx"

#include <string>

struct CronPartitionConfig {
    /**
     * Partition name.  Empty when not specified.
     */
    std::string name;

    std::string database, database_schema;

    std::string translation_socket;

    AllocatedSocketAddress qmqp_server;

    explicit CronPartitionConfig(std::string &&_name):name(std::move(_name)) {}

    void Check() const;
};

#endif
