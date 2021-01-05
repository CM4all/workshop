/*
 * Copyright 2006-2021 CM4all GmbH
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
#include "io/UniqueFileDescriptor.hxx"

PipeCaptureBuffer::PipeCaptureBuffer(EventLoop &event_loop,
				     UniqueFileDescriptor _fd,
				     size_t capacity) noexcept
	:event(event_loop, BIND_THIS_METHOD(OnSocket),
	       SocketDescriptor::FromFileDescriptor(_fd.Release())),
	 buffer(capacity)
{
	event.ScheduleRead();
}

void
PipeCaptureBuffer::OnSocket(unsigned) noexcept
{
	FileDescriptor fd(event.GetSocket().ToFileDescriptor());

	auto w = buffer.Write();
	if (!w.empty()) {
		ssize_t nbytes = fd.Read(w.data, w.size);
		if (nbytes <= 0) {
			Close();
			OnEnd();
			return;
		}

		buffer.Append(nbytes);
		OnAppend();

		if (IsFull())
			OnEnd();
	} else {
		/* buffer is full: discard data to keep the pipe from blocking
		   the other end */
		char discard[4096];
		if (fd.Read(discard, sizeof(discard)) <= 0)
			Close();
	}
}
