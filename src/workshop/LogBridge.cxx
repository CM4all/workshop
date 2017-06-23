/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "LogBridge.hxx"
#include "util/StringView.hxx"

LogBridge::LogBridge(EventLoop &event_loop,
                     UniqueFileDescriptor &&read_pipe_fd,
                     const char *host_and_port,
                     const char *me, const char *ident,
                     int facility)
    :reader(event_loop, std::move(read_pipe_fd),
            BIND_THIS_METHOD(OnStderrLine)),
     client(host_and_port, me, ident, facility)
{
}

bool
LogBridge::OnStderrLine(WritableBuffer<char> line)
{
    if (line.IsNull())
        return false;

    // TODO: strip non-ASCII characters
    client.Log(6, {line.data, line.size});
    return true;
}
