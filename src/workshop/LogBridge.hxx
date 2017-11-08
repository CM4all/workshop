/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_LOG_BRIDGE_HXX
#define WORKSHOP_LOG_BRIDGE_HXX

#include "event/PipeLineReader.hxx"

#include <memory>

class SyslogClient;

class LogBridge {
    const std::string plan_name, job_id;

    PipeLineReader reader;
    std::unique_ptr<SyslogClient> syslog;
    bool enable_journal = false;

    std::string buffer;
    size_t max_buffer_size = 0;

public:
    LogBridge(EventLoop &event_loop,
              const char *_plan_name, const char *_job_id,
              UniqueFileDescriptor read_pipe_fd);
    ~LogBridge();

    void CreateSyslog(const char *host_and_port,
                      const char *me,
                      int facility);

    void EnableBuffer(size_t max_size) {
        max_buffer_size = max_size;
    }

    const char *GetBuffer() const {
        return max_buffer_size > 0
            ? buffer.c_str()
            : nullptr;
    }

    void EnableJournal() {
        enable_journal = true;
    }

    void Flush() {
        reader.Flush();
    }

private:
    bool OnStderrLine(WritableBuffer<char> line);
};

#endif
