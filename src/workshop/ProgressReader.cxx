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

#include "ProgressReader.hxx"

ProgressReader::ProgressReader(EventLoop &event_loop,
                               UniqueFileDescriptor _fd,
                               Callback _callback)
    :fd(std::move(_fd)),
     event(event_loop, fd.Get(), SocketEvent::READ|SocketEvent::PERSIST,
           BIND_THIS_METHOD(PipeReady)),
     callback(_callback)
{
    event.Add();
}

void
ProgressReader::PipeReady(unsigned)
{
    char buffer[512];
    ssize_t nbytes, i;
    unsigned new_progress = 0, p;

    nbytes = fd.Read(buffer, sizeof(buffer));
    if (nbytes <= 0) {
        event.Delete();
        fd.Close();
        return;
    }

    for (i = 0; i < nbytes; ++i) {
        char ch = buffer[i];

        if (ch >= '0' && ch <= '9' &&
            stdout_buffer.size() < stdout_buffer.capacity() - 1) {
            stdout_buffer.push_back(ch);
        } else {
            if (!stdout_buffer.empty()) {
                stdout_buffer.push_back('\0');
                p = (unsigned)strtoul(stdout_buffer.begin(), nullptr, 10);
                if (p <= 100)
                    new_progress = p;
            }

            stdout_buffer.clear();
        }
    }

    if (new_progress > 0 && new_progress != last_progress) {
        callback(new_progress);
        last_progress = new_progress;
    }
}
