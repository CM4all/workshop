/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_PARTITION_HXX
#define CRON_PARTITION_HXX

#include "Queue.hxx"
#include "Workplace.hxx"
#include "Config.hxx"
#include "spawn/ExitListener.hxx"
#include "util/BindMethod.hxx"

class CronInstance;
class SpawnService;

class CronPartition final : ExitListener {
    CronInstance &instance;

    const char *const translation_socket;

    CronQueue queue;

    CronWorkplace workplace;

    BoundMethod<void()> empty_callback;

public:
    CronPartition(CronInstance &instance,
                  SpawnService &_spawn_service,
                  const CronConfig &root_config,
                  const CronConfig::Partition &config,
                  BoundMethod<void()> _empty_callback);

    bool IsIdle() const {
        return workplace.IsEmpty();
    }

    void Start() {
        queue.Connect();
    }

    void Close() {
        queue.Close();
    }

    void BeginShutdown() {
        queue.Disable();
    }

private:
    void OnJob(CronJob &&job);

    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;
};

#endif
