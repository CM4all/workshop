/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_OPERATOR_HXX
#define CRON_OPERATOR_HXX

#include "Job.hxx"
#include "event/TimerEvent.hxx"

#include <boost/intrusive/list.hpp>

#include <string>

class ChildProcessRegistry;
class CronQueue;
class CronWorkplace;

/**
 * A #CronJob being executed.
 */
class CronOperator
    : public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>> {

protected:
    CronQueue &queue;
    CronWorkplace &workplace;
    const CronJob job;

    const std::string start_time;

    TimerEvent timeout_event;

public:
    CronOperator(CronQueue &_queue, CronWorkplace &_workplace, CronJob &&_job,
                 std::string &&_start_time) noexcept;

    virtual ~CronOperator() {}

    CronOperator(const CronOperator &other) = delete;
    CronOperator &operator=(const CronOperator &other) = delete;

    EventLoop &GetEventLoop();

    /**
     * Cancel job execution, e.g. by sending SIGTERM to the child
     * process.  This also abandons the child process, i.e. after this
     * method returns, cancellation can be considered complete, even
     * if the child process continues to run (because it ignores the
     * kill signal).
     */
    virtual void Cancel() = 0;

private:
    void OnTimeout();
};

#endif
