/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_OPERATOR_HXX
#define WORKSHOP_OPERATOR_HXX

#include "spawn/ExitListener.hxx"
#include "event/SocketEvent.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "Job.hxx"

#include <boost/intrusive/list.hpp>

#include <memory>
#include <string>
#include <list>

struct Plan;
class Workplace;
class SyslogClient;
struct Job;

/** an operator is a job being executed */
struct Operator final
    : public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>,
      public ExitListener {

    Workplace &workplace;
    Job job;
    std::shared_ptr<Plan> plan;
    pid_t pid;

    UniqueFileDescriptor stdout_fd;
    SocketEvent stdout_event;
    char stdout_buffer[64];
    size_t stdout_length = 0;
    unsigned progress = 0;

    UniqueFileDescriptor stderr_fd;
    SocketEvent stderr_event;
    char stderr_buffer[512];
    size_t stderr_length = 0;
    std::unique_ptr<SyslogClient> syslog;

    Operator(EventLoop &event_loop, Workplace &_workplace, const Job &_job,
             const std::shared_ptr<Plan> &_plan);

    Operator(const Operator &other) = delete;

    ~Operator();

    Operator &operator=(const Operator &other) = delete;

    void SetOutput(UniqueFileDescriptor &&fd);
    void SetSyslog(UniqueFileDescriptor &&fd);

    void Expand(std::list<std::string> &args) const;

private:
    void OnOutputReady(short events);
    void OnErrorReady(short events);

public:
    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;
};

#endif
