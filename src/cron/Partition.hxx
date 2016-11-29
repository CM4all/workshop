/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_PARTITION_HXX
#define CRON_PARTITION_HXX

#include "Queue.hxx"
#include "Config.hxx"

class CronInstance;

class CronPartition final {
    CronInstance &instance;

    const char *const translation_socket;

    CronQueue queue;

public:
    CronPartition(CronInstance &instance,
                  const CronConfig &root_config,
                  const CronConfig::Partition &config);

    void Start() {
        queue.Connect();
    }

    void Enable() {
        queue.Enable();
    }

    void Disable() {
        queue.Disable();
    }

    void Close() {
        queue.Close();
    }

private:
    void OnJob(CronJob &&job);
};

#endif
