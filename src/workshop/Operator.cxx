/*
 * Manage operator processes.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Operator.hxx"
#include "ProgressReader.hxx"
#include "Expand.hxx"
#include "Workplace.hxx"
#include "Plan.hxx"
#include "Job.hxx"
#include "SyslogClient.hxx"
#include "system/Error.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringView.hxx"

#include <daemon/log.h>

#include <map>
#include <string>

#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>

WorkshopOperator::WorkshopOperator(EventLoop &_event_loop,
                                   WorkshopWorkplace &_workplace,
                                   const WorkshopJob &_job,
                                   const std::shared_ptr<Plan> &_plan)
    :event_loop(_event_loop), workplace(_workplace), job(_job), plan(_plan),
     stderr_event(event_loop, BIND_THIS_METHOD(OnErrorReady))
{
}

WorkshopOperator::~WorkshopOperator()
{
    if (stderr_fd.IsDefined())
        stderr_event.Delete();
}

void
WorkshopOperator::OnProgress(unsigned progress)
{
    job.SetProgress(progress, plan->timeout.c_str());
}

void
WorkshopOperator::SetOutput(UniqueFileDescriptor &&fd)
{
    assert(fd.IsDefined());
    assert(!progress_reader);

    progress_reader.reset(new ProgressReader(event_loop, std::move(fd),
                                             BIND_THIS_METHOD(OnProgress)));

}

void
WorkshopOperator::OnErrorReady(unsigned)
{
    assert(syslog != nullptr);

    char buffer[512];
    ssize_t nbytes = stderr_fd.Read(buffer, sizeof(buffer));
    if (nbytes <= 0) {
        stderr_event.Delete();
        stderr_fd.Close();
        return;
    }

    for (ssize_t i = 0; i < nbytes; ++i) {
        char ch = buffer[i];

        if (ch == '\r' || ch == '\n') {
            if (!stderr_buffer.empty()) {
                syslog->Log(6, {stderr_buffer.begin(), stderr_buffer.size()});
            }

            stderr_buffer.clear();
        } else if (ch > 0 && (ch & ~0x7f) == 0 &&
                   !stderr_buffer.full()) {
            stderr_buffer.push_back(ch);
        }
    }
}

void
WorkshopOperator::SetSyslog(UniqueFileDescriptor &&fd)
{
    assert(fd.IsDefined());
    assert(!stderr_fd.IsDefined());

    stderr_fd = std::move(fd);
    stderr_event.Set(stderr_fd.Get(), SocketEvent::READ|SocketEvent::PERSIST);
    stderr_event.Add();
}

UniqueFileDescriptor
WorkshopOperator::CreateSyslogClient(const char *me, const char *ident,
                                     int facility,
                                     const char *host_and_port)
{
    try {
        syslog.reset(new SyslogClient(host_and_port, me, ident, facility));
    } catch (const std::runtime_error &e) {
        std::throw_with_nested(FormatRuntimeError("syslog_open(%s) failed",
                                                  host_and_port));
    }

    UniqueFileDescriptor stderr_r, stderr_w;
    if (!UniqueFileDescriptor::CreatePipe(stderr_r, stderr_w))
        throw MakeErrno("pipe() failed");

    SetSyslog(std::move(stderr_r));
    return stderr_w;
}

void
WorkshopOperator::Expand(std::list<std::string> &args) const
{
    assert(!args.empty());

    StringMap vars;
    vars.emplace("0", args.front());
    vars.emplace("NODE", workplace.GetNodeName());
    vars.emplace("JOB", job.id);
    vars.emplace("PLAN", job.plan_name);

    for (auto &i : args)
        ::Expand(i, vars);
}

void
WorkshopOperator::OnChildProcessExit(int status)
{
    int exit_status = WEXITSTATUS(status);

    if (WIFSIGNALED(status)) {
        daemon_log(1, "job %s (pid %d) died from signal %d%s\n",
                   job.id.c_str(), pid,
                   WTERMSIG(status),
                   WCOREDUMP(status) ? " (core dumped)" : "");
        exit_status = -1;
    } else if (exit_status == 0)
        daemon_log(3, "job %s (pid %d) exited with success\n",
                   job.id.c_str(), pid);
    else
        daemon_log(2, "job %s (pid %d) exited with status %d\n",
                   job.id.c_str(), pid,
                   exit_status);

    job.SetDone(exit_status);

    workplace.OnExit(this);
}
