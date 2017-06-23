/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_LOG_BRIDGE_HXX
#define WORKSHOP_LOG_BRIDGE_HXX

#include "SyslogClient.hxx"
#include "event/PipeLineReader.hxx"
#include "util/StaticArray.hxx"

class LogBridge {
    PipeLineReader reader;
    SyslogClient client;

    StaticArray<char, 1024> buffer;

public:
    LogBridge(EventLoop &event_loop, UniqueFileDescriptor &&read_pipe_fd,
              const char *host_and_port,
              const char *me, const char *ident,
              int facility)
        :reader(event_loop, std::move(read_pipe_fd),
                BIND_THIS_METHOD(OnStderrLine)),
         client(host_and_port, me, ident, facility) {}

    void Flush() {
        reader.Flush();
    }

private:
    bool OnStderrLine(WritableBuffer<char> line);
};

#endif
