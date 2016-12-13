/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_OPERATOR_HXX
#define CRON_OPERATOR_HXX

#include "spawn/ExitListener.hxx"
#include "Job.hxx"

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

public:
    CronOperator(CronQueue &_queue, CronWorkplace &_workplace, CronJob &&_job);

    CronOperator(const CronOperator &other) = delete;
    CronOperator &operator=(const CronOperator &other) = delete;

    void Spawn(PreparedChildProcess &&p);

public:
    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;
};

#endif
