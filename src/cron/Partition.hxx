/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_PARTITION_HXX
#define CRON_PARTITION_HXX

#include "Queue.hxx"
#include "Workplace.hxx"
#include "spawn/ExitListener.hxx"
#include "util/BindMethod.hxx"

struct Config;
struct CronPartitionConfig;
class EventLoop;
class SpawnService;

class CronPartition final : ExitListener {
    const char *const translation_socket;

    CronQueue queue;

    CronWorkplace workplace;

    BoundMethod<void()> idle_callback;

public:
    CronPartition(EventLoop &event_loop,
                  SpawnService &_spawn_service,
                  const Config &root_config,
                  const CronPartitionConfig &config,
                  BoundMethod<void()> _idle_callback);

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
