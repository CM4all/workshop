/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_OPERATOR_HXX
#define WORKSHOP_OPERATOR_HXX

#include "spawn/ExitListener.hxx"
#include "event/SocketEvent.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/StaticArray.hxx"
#include "Job.hxx"

#include <boost/intrusive/list.hpp>

#include <memory>
#include <string>
#include <list>

struct Plan;
class WorkshopWorkplace;
class SyslogClient;

/** an operator is a job being executed */
struct WorkshopOperator final
    : public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>,
      public ExitListener {

    WorkshopWorkplace &workplace;
    WorkshopJob job;
    std::shared_ptr<Plan> plan;
    pid_t pid;

    UniqueFileDescriptor stdout_fd;
    SocketEvent stdout_event;
    StaticArray<char, 64> stdout_buffer;
    unsigned progress = 0;

    UniqueFileDescriptor stderr_fd;
    SocketEvent stderr_event;
    StaticArray<char, 64> stderr_buffer;
    std::unique_ptr<SyslogClient> syslog;

    WorkshopOperator(EventLoop &event_loop,
                     WorkshopWorkplace &_workplace, const WorkshopJob &_job,
                     const std::shared_ptr<Plan> &_plan);

    WorkshopOperator(const WorkshopOperator &other) = delete;

    ~WorkshopOperator();

    WorkshopOperator &operator=(const WorkshopOperator &other) = delete;

    void SetOutput(UniqueFileDescriptor &&fd);
    void SetSyslog(UniqueFileDescriptor &&fd);

    /**
     * @return a writable pipe to be attached to the child's stderr
     */
    UniqueFileDescriptor CreateSyslogClient(const char *me, const char *ident,
                                            int facility,
                                            const char *host_and_port);

    void Expand(std::list<std::string> &args) const;

private:
    void OnOutputReady(unsigned events);
    void OnErrorReady(unsigned events);

public:
    /* virtual methods from ExitListener */
    void OnChildProcessExit(int status) override;
};

#endif
