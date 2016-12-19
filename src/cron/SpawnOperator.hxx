/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_SPAWN_OPERATOR_HXX
#define CRON_SPAWN_OPERATOR_HXX

#include "Operator.hxx"
#include "spawn/ExitListener.hxx"

struct PreparedChildProcess;

/**
 * A #CronJob being executed as a spawned child process.
 */
class CronSpawnOperator final
    : public CronOperator,
      ExitListener {

    int pid = -1;

public:
    CronSpawnOperator(CronQueue &_queue, CronWorkplace &_workplace, CronJob &&_job,
                      std::string &&_start_time) noexcept
        :CronOperator(_queue, _workplace,
                      std::move(_job),
                      std::move(_start_time)) {}

    void Spawn(PreparedChildProcess &&p);

    void Cancel() override;

public:
    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;

private:
    void OnTimeout();
};

#endif
