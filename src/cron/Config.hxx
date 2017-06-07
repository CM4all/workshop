/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_CONFIG_HXX
#define CRON_CONFIG_HXX

#include "net/AllocatedSocketAddress.hxx"

#include <string>

struct CronPartitionConfig {
    std::string database, database_schema;

    std::string translation_socket;

    AllocatedSocketAddress qmqp_server;

    void Check() const;
};

#endif
