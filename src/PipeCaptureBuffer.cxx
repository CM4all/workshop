/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "PipeCaptureBuffer.hxx"

PipeCaptureBuffer::PipeCaptureBuffer(EventLoop &event_loop, UniqueFileDescriptor &&_fd)
        :fd(std::move(_fd)),
         event(event_loop, fd.Get(), EV_READ|EV_PERSIST,
               BIND_THIS_METHOD(OnSocket))
{
    event.Add();
}

PipeCaptureBuffer::~PipeCaptureBuffer()
{
    event.Delete();
}

void
PipeCaptureBuffer::OnSocket(short)
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
