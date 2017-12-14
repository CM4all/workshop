/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_OPERATOR_HXX
#define WORKSHOP_OPERATOR_HXX

#include "spawn/ExitListener.hxx"
#include "event/TimerEvent.hxx"
#include "event/net/UdpHandler.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/Logger.hxx"
#include "Job.hxx"
#include "LogBridge.hxx"

#include <boost/intrusive/list.hpp>

#include <memory>
#include <string>
#include <vector>
#include <list>
#include <chrono>

struct Plan;
class WorkshopWorkplace;
class ProgressReader;
class UdpListener;
class UniqueSocketDescriptor;

/** an operator is a job being executed */
class WorkshopOperator final
    : public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>,
      public ExitListener, UdpHandler, LoggerDomainFactory {

    EventLoop &event_loop;

    WorkshopWorkplace &workplace;
    WorkshopJob job;
    std::shared_ptr<Plan> plan;
    int pid = -1;

    bool exited = false;

    /**
     * Shall the job be executed again?
     *
     * The value is negative if the job shall NOT be executed again,
     * and positive to delay the repeated execution.
     */
    std::chrono::seconds again = std::chrono::seconds(-1);

    LazyDomainLogger logger;

    TimerEvent timeout_event;

    std::unique_ptr<ProgressReader> progress_reader;

    std::unique_ptr<UdpListener> control_channel;

    LogBridge log;

public:
    WorkshopOperator(EventLoop &_event_loop,
                     WorkshopWorkplace &_workplace, const WorkshopJob &_job,
                     const std::shared_ptr<Plan> &_plan,
                     UniqueFileDescriptor stderr_read_pipe,
                     UniqueSocketDescriptor control_socket,
                     size_t max_log_buffer,
                     bool enable_journal);

    WorkshopOperator(const WorkshopOperator &other) = delete;

    ~WorkshopOperator();

    WorkshopOperator &operator=(const WorkshopOperator &other) = delete;

    const Plan &GetPlan() const {
        return *plan;
    }

    const std::string &GetPlanName() const {
        return job.plan_name;
    }

    void SetPid(int _pid) {
        pid = _pid;
    }

    void SetOutput(UniqueFileDescriptor fd);

    void CreateSyslogClient(const char *me,
                            int facility,
                            const char *host_and_port);

    void Expand(std::list<std::string> &args) const;

private:
    void ScheduleTimeout();
    void OnTimeout();
    void OnProgress(unsigned progress);

public:
    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;

private:
    /* virtual methods from LoggerDomainFactory */
    std::string MakeLoggerDomain() const noexcept override;

    /* virtual methods from UdpHandler */
    bool OnUdpDatagram(const void *data, size_t length,
                       SocketAddress address, int uid) override;
    void OnUdpError(std::exception_ptr ep) noexcept override;

    bool OnControl(std::vector<std::string> &&args) noexcept;
};

#endif
