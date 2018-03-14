/*
 * Copyright 2006-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PipeCaptureBuffer.hxx"

PipeCaptureBuffer::PipeCaptureBuffer(EventLoop &event_loop,
                                     UniqueFileDescriptor _fd,
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
    if (!w.empty()) {
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
