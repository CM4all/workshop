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
#include "LogBridge.hxx"
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
                                   const std::shared_ptr<Plan> &_plan,
                                   UniqueFileDescriptor &&stderr_read_pipe,
                                   size_t max_log_buffer,
                                   bool enable_journal)
    :event_loop(_event_loop), workplace(_workplace), job(_job), plan(_plan),
     log(event_loop, job.plan_name.c_str(), job.id.c_str(),
         std::move(stderr_read_pipe))
{
    if (max_log_buffer > 0)
        log.EnableBuffer(max_log_buffer);

    if (enable_journal)
        log.EnableJournal();
}

WorkshopOperator::~WorkshopOperator() = default;

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
WorkshopOperator::CreateSyslogClient(const char *me,
                                     int facility,
                                     const char *host_and_port)
{
    try{
        log.CreateSyslog(host_and_port, me, facility);
    } catch (const std::runtime_error &e) {
        std::throw_with_nested(FormatRuntimeError("syslog_open(%s) failed",
                                                  host_and_port));
    }
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
    log.Flush();

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

    job.SetDone(exit_status, log.GetBuffer());

    workplace.OnExit(this);
}
