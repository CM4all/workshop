/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_OPERATOR_HXX
#define CRON_OPERATOR_HXX

#include "Job.hxx"
#include "spawn/ExitListener.hxx"
#include "event/TimerEvent.hxx"

#include <boost/intrusive/list.hpp>

#include <string>

struct PreparedChildProcess;
class ChildProcessRegistry;
class CronQueue;
class CronWorkplace;

/**
 * A #CronJob being executed.
 */
class CronOperator final
    : public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>,
      ExitListener {

    CronQueue &queue;
    CronWorkplace &workplace;
    const CronJob job;

    const std::string start_time;

    int pid = -1;

    TimerEvent timeout_event;

public:
    CronOperator(CronQueue &_queue, CronWorkplace &_workplace, CronJob &&_job,
                 std::string &&_start_time) noexcept;

    CronOperator(const CronOperator &other) = delete;
    CronOperator &operator=(const CronOperator &other) = delete;

    void Spawn(PreparedChildProcess &&p);

    /**
     * Cancel job execution, e.g. by sending SIGTERM to the child
     * process.  This also abandons the child process, i.e. after this
     * method returns, cancellation can be considered complete, even
     * if the child process continues to run (because it ignores the
     * kill signal).
     */
    void Cancel();

public:
    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;

private:
    void OnTimeout();
};

#endif
