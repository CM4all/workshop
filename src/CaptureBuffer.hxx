/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CAPTURE_BUFFER_HXX
#define CAPTURE_BUFFER_HXX

#include "io/UniqueFileDescriptor.hxx"
#include "event/SocketEvent.hxx"
#include "util/WritableBuffer.hxx"

#include <array>

/**
 * Capture up to 8 kB of data from a pipe asynchronously.  This is
 * useful to capture the output of a child process.
 */
class CaptureBuffer final {
    UniqueFileDescriptor fd;
    SocketEvent event;

    size_t length = 0;
    std::array<char, 8192> data;

public:
    explicit CaptureBuffer(EventLoop &event_loop, UniqueFileDescriptor &&_fd);
    ~CaptureBuffer();

    WritableBuffer<char> GetData() {
        return {&data.front(), length};
    }

    /**
     * Convert all non-printable and non-ASCII characters except for
     * CR/LF and tab to a space and null-terminate the string.  This
     * modifies this object's buffer.
     *
     * @return the null-terminated ASCII string
     */
    char *NormalizeASCII();

private:
    void Close() {
        event.Delete();
        fd.Close();
    }

    void OnSocket(short events);
};

#endif
