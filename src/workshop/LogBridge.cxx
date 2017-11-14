/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "LogBridge.hxx"
#include "SyslogClient.hxx"
#include "util/StringView.hxx"

#include <systemd/sd-journal.h>

LogBridge::LogBridge(EventLoop &event_loop,
                     const char *_plan_name, const char *_job_id,
                     UniqueFileDescriptor read_pipe_fd)
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
LogBridge::OnStderrLine(WritableBuffer<char> line) noexcept
{
    if (line.IsNull())
        return false;

    // TODO: strip non-ASCII characters

    if (max_buffer_size > 0 && buffer.length() < max_buffer_size - 1) {
        buffer.append(line.data,
                      std::min(line.size, max_buffer_size - 1 - buffer.length()));
        buffer.push_back('\n');
    }

    if (syslog)
        syslog->Log(6, {line.data, line.size});

    if (enable_journal)
        sd_journal_send("MESSAGE=%.*s", int(line.size), line.data,
                        "WORKSHOP_PLAN=%s", plan_name.c_str(),
                        "WORKSHOP_JOB=%s", job_id.c_str(),
                        nullptr);

    if (max_buffer_size == 0 && !syslog && !enable_journal)
        fprintf(stderr, "[%s:%s] %.*s\n", plan_name.c_str(), job_id.c_str(),
                int(line.size), line.data);

    return true;
}
