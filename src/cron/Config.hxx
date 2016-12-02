/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_CONFIG_HXX
#define CRON_CONFIG_HXX

#include <string>

struct CronPartitionConfig {
    std::string database, database_schema;

    std::string translation_socket;

    void Check() const;
};

#endif
