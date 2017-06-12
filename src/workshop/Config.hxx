/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_CONFIG_HXX
#define WORKSHOP_CONFIG_HXX

#include <string>

struct WorkshopPartitionConfig {
    std::string database, database_schema;

    size_t max_log = 8192;

    bool enable_journal = false;

    WorkshopPartitionConfig() = default;
    explicit WorkshopPartitionConfig(const char *_database)
        :database(_database) {}

    void Check() const;
};

#endif
