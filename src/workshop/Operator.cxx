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
#include "event/net/UdpListener.hxx"
#include "net/SocketAddress.hxx"
#include "system/Error.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringView.hxx"
#include "util/StringFormat.hxx"
#include "util/IterableSplitString.hxx"

#include <map>

#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>

WorkshopOperator::WorkshopOperator(EventLoop &_event_loop,
                                   WorkshopWorkplace &_workplace,
                                   const WorkshopJob &_job,
                                   const std::shared_ptr<Plan> &_plan,
                                   UniqueFileDescriptor stderr_read_pipe,
                                   UniqueSocketDescriptor control_socket,
                                   size_t max_log_buffer,
                                   bool enable_journal)
    :event_loop(_event_loop), workplace(_workplace), job(_job), plan(_plan),
     logger(*this),
     timeout_event(event_loop, BIND_THIS_METHOD(OnTimeout)),
     control_channel(control_socket.IsDefined()
                     ? new UdpListener(event_loop, std::move(control_socket),
                                       *this)
                     : nullptr),
     log(event_loop, job.plan_name.c_str(), job.id.c_str(),
         std::move(stderr_read_pipe))
{
    ScheduleTimeout();

    if (max_log_buffer > 0)
        log.EnableBuffer(max_log_buffer);

    if (enable_journal)
        log.EnableJournal();
}

WorkshopOperator::~WorkshopOperator()
{
    timeout_event.Cancel();
}

template<class Rep, class Period>
static struct timeval
ToTimeval(std::chrono::duration<Rep, Period> src)
{
    auto s = std::chrono::duration_cast<std::chrono::seconds>(src);
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(src);
    us -= s;

    struct timeval tv;
    tv.tv_sec = s.count();
    tv.tv_usec = us.count();
    return tv;
}

void
WorkshopOperator::ScheduleTimeout()
{
    const auto t = plan->parsed_timeout;
    if (t > std::chrono::seconds(0))
        timeout_event.Add(ToTimeval(t));
}

void
WorkshopOperator::OnTimeout()
{
    logger(2, "timed out; sending SIGTERM");

    job.SetDone(-1, "Timeout");

    workplace.OnTimeout(this, pid);
}

void
WorkshopOperator::OnProgress(unsigned progress)
{
    if (exited)
        /* after the child process has exited, it's pointless to
           update the progress because it will be set to 100% anyway;
           this state can occur in OnChildProcessExit() during the
           LogBridge::Flush() call */
        return;

    job.SetProgress(progress, plan->timeout.c_str());

    /* refresh the timeout */
    ScheduleTimeout();
}

void
WorkshopOperator::SetOutput(UniqueFileDescriptor fd)
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
    exited = true;

    log.Flush();

    int exit_status = WEXITSTATUS(status);

    if (WIFSIGNALED(status)) {
        logger(1, "died from signal ",
               WTERMSIG(status),
               WCOREDUMP(status) ? " (core dumped)" : "");
        exit_status = -1;
    } else if (exit_status == 0)
        logger(3, "exited with success");
    else
        logger(2, "exited with status ", exit_status);

    job.SetDone(exit_status, log.GetBuffer());

    workplace.OnExit(this);
}

std::string
WorkshopOperator::MakeLoggerDomain() const noexcept
{
    return StringFormat<64>("job %s pid=%d", job.id.c_str(), int(pid)).c_str();
}

gcc_pure
static StringView
FirstLine(StringView s) noexcept
{
    const char *newline = s.Find('\n');
    if (newline != nullptr)
        s.SetEnd(newline);
    return s;
}

gcc_pure
static std::vector<std::string>
SplitArgs(StringView s) noexcept
{
    std::vector<std::string> result;
    for (StringView i : IterableSplitString(s, ' '))
        result.emplace_back(i.data, i.size);
    return result;
}

bool
WorkshopOperator::OnUdpDatagram(const void *data, size_t length,
                                SocketAddress, int)
{
    if (length == 0) {
        control_channel.reset();
        return false;
    }

    const StringView payload((const char *)data, length);
    return OnControl(SplitArgs(FirstLine(payload)));
}

void
WorkshopOperator::OnUdpError(std::exception_ptr ep) noexcept
{
    control_channel.reset();
    logger(3, "error on control channel", ep);
}

bool
WorkshopOperator::OnControl(std::vector<std::string> &&args) noexcept
{
    const auto &cmd = args.front();

    if (cmd == "progress") {
        if (args.size() != 2) {
            logger(2, "malformed progress command on control channel");
            return true;
        }

        char *endptr;
        auto progress = strtoul(args[1].c_str(), &endptr, 10);
        if (*endptr != 0 || progress > 100) {
            logger(2, "malformed progress command on control channel");
            return true;
        }

        OnProgress(progress);

        return true;
    } else {
        logger(2, "unknown command on control channel: '", cmd, "'");
        return true;
    }
}
