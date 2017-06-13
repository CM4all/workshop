/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef PIPE_CAPTURE_BUFFER_HXX
#define PIPE_CAPTURE_BUFFER_HXX

#include "CaptureBuffer.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "event/SocketEvent.hxx"

/**
 * Capture up to 8 kB of data from a pipe asynchronously.  This is
 * useful to capture the output of a child process.
 */
class PipeCaptureBuffer final {
    UniqueFileDescriptor fd;
    SocketEvent event;

    CaptureBuffer buffer;

public:
    explicit PipeCaptureBuffer(EventLoop &event_loop,
                               UniqueFileDescriptor &&_fd,
                               size_t capacity);
    ~PipeCaptureBuffer();

    WritableBuffer<char> GetData() {
        return buffer.GetData();
    }

    char *NormalizeASCII() {
        return buffer.NormalizeASCII();
    }

private:
    void Close() {
        event.Delete();
        fd.Close();
    }

    void OnSocket(unsigned events);
};

#endif
