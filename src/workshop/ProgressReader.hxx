/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_PROGRESS_READER_HXX
#define WORKSHOP_PROGRESS_READER_HXX

#include "event/SocketEvent.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/BindMethod.hxx"
#include "util/StaticArray.hxx"

class ProgressReader {
    UniqueFileDescriptor fd;
    SocketEvent event;
    StaticArray<char, 64> stdout_buffer;
    unsigned last_progress = 0;

    typedef BoundMethod<void(unsigned value)> Callback;
    const Callback callback;

public:
    ProgressReader(EventLoop &event_loop,
                   UniqueFileDescriptor &&_fd,
                   Callback _callback);

    ~ProgressReader() {
        event.Delete();
    }

private:
    void PipeReady(unsigned);
};

#endif
