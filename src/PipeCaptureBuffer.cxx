/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "PipeCaptureBuffer.hxx"

PipeCaptureBuffer::PipeCaptureBuffer(EventLoop &event_loop,
                                     UniqueFileDescriptor &&_fd,
                                     size_t capacity)
        :fd(std::move(_fd)),
         event(event_loop, fd.Get(), SocketEvent::READ|SocketEvent::PERSIST,
               BIND_THIS_METHOD(OnSocket)),
         buffer(capacity)
{
    event.Add();
}

PipeCaptureBuffer::~PipeCaptureBuffer()
{
    event.Delete();
}

void
PipeCaptureBuffer::OnSocket(unsigned)
{
    auto w = buffer.Write();
    if (!w.IsEmpty()) {
        ssize_t nbytes = fd.Read(w.data, w.size);
        if (nbytes <= 0) {
            Close();
            return;
        }

        buffer.Append(nbytes);
    } else {
        /* buffer is full: discard data to keep the pipe from blocking
           the other end */
        char discard[4096];
        if (fd.Read(discard, sizeof(discard)) <= 0)
            Close();
    }
}
