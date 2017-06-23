/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "LogBridge.hxx"
#include "SyslogClient.hxx"
#include "util/StringView.hxx"

LogBridge::LogBridge(EventLoop &event_loop,
                     const char *_plan_name, const char *_job_id,
                     UniqueFileDescriptor &&read_pipe_fd)
    :plan_name(_plan_name), job_id(_job_id),
     reader(event_loop, std::move(read_pipe_fd),
            BIND_THIS_METHOD(OnStderrLine))
{
}

LogBridge::~LogBridge() = default;

void
LogBridge::CreateSyslog(const char *host_and_port,
                        const char *me,
                        int facility)
{
    char ident[256];
    snprintf(ident, sizeof(ident), "%s[%s]",
             plan_name.c_str(), job_id.c_str());

    syslog.reset(new SyslogClient(host_and_port, me, ident, facility));
}

bool
LogBridge::OnStderrLine(WritableBuffer<char> line)
{
    if (line.IsNull())
        return false;

    // TODO: strip non-ASCII characters
    if (syslog)
        syslog->Log(6, {line.data, line.size});
    else
        fprintf(stderr, "[%s:%s] %.*s\n", plan_name.c_str(), job_id.c_str(),
                int(line.size), line.data);

    return true;
}
