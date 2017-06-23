/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_LOG_BRIDGE_HXX
#define WORKSHOP_LOG_BRIDGE_HXX

#include "event/PipeLineReader.hxx"
#include "util/StaticArray.hxx"

#include <memory>

class SyslogClient;

class LogBridge {
    PipeLineReader reader;
    std::unique_ptr<SyslogClient> syslog;

    StaticArray<char, 1024> buffer;

public:
    LogBridge(EventLoop &event_loop, UniqueFileDescriptor &&read_pipe_fd);
    ~LogBridge();

    void CreateSyslog(const char *host_and_port,
                      const char *me, const char *ident,
                      int facility);

    void Flush() {
        reader.Flush();
    }

private:
    bool OnStderrLine(WritableBuffer<char> line);
};

#endif
