/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_OPERATOR_HXX
#define WORKSHOP_OPERATOR_HXX

#include "spawn/ExitListener.hxx"
#include "Job.hxx"

#include <boost/intrusive/list.hpp>

struct PreparedChildProcess;
class ChildProcessRegistry;
class CronQueue;
class CronWorkplace;

/**
 * A #CronJob being executed.
 */
class Operator final
    : public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>,
      ExitListener {

    CronQueue &queue;
    CronWorkplace &workplace;
    const CronJob job;

    int pid = -1;

public:
    Operator(CronQueue &_queue, CronWorkplace &_workplace, CronJob &&_job);

    Operator(const Operator &other) = delete;
    Operator &operator=(const Operator &other) = delete;

    void Spawn(PreparedChildProcess &&p);

public:
    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;
};

#endif
