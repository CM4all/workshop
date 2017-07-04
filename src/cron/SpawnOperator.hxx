/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_SPAWN_OPERATOR_HXX
#define CRON_SPAWN_OPERATOR_HXX

#include "Operator.hxx"
#include "spawn/ExitListener.hxx"

#include <memory>

struct PreparedChildProcess;
class SpawnService;
class PipeCaptureBuffer;

/**
 * A #CronJob being executed as a spawned child process.
 */
class CronSpawnOperator final
    : public CronOperator,
    ExitListener {

    SpawnService &spawn_service;

    int pid = -1;

    std::unique_ptr<PipeCaptureBuffer> output_capture;

public:
    CronSpawnOperator(CronQueue &_queue, CronWorkplace &_workplace,
                      SpawnService &_spawn_service,
                      CronJob &&_job,
                      std::string &&_start_time) noexcept;
    ~CronSpawnOperator() override;

    void Spawn(PreparedChildProcess &&p);

    void Cancel() override;

public:
    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;
};

#endif
