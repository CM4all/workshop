/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_PARTITION_HXX
#define CRON_PARTITION_HXX

#include "Queue.hxx"
#include "Workplace.hxx"
#include "spawn/ExitListener.hxx"
#include "io/Logger.hxx"
#include "util/BindMethod.hxx"

#include <memory>

struct Config;
struct CronPartitionConfig;
class EventLoop;
class SpawnService;
class EmailService;

class CronPartition final : ExitListener {
    const char *const name;
    const char *const tag;
    const char *const translation_socket;

    Logger logger;

    std::unique_ptr<EmailService> email_service;

    CronQueue queue;

    CronWorkplace workplace;

    BoundMethod<void()> idle_callback;

public:
    CronPartition(EventLoop &event_loop,
                  SpawnService &_spawn_service,
                  CurlGlobal &_curl,
                  const Config &root_config,
                  const CronPartitionConfig &config,
                  BoundMethod<void()> _idle_callback);

    ~CronPartition();

    bool IsIdle() const {
        return workplace.IsEmpty();
    }

    void Start() {
        queue.Connect();
    }

    void Close() {
        queue.Close();
    }

    void BeginShutdown();

    void DisableQueue() {
        queue.DisableAdmin();
    }

    void EnableQueue() {
        queue.EnableAdmin();
    }

private:
    void OnJob(CronJob &&job);

    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;
};

#endif
