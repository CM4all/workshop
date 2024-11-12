// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PipeCaptureBuffer.hxx"
#include "io/UniqueFileDescriptor.hxx"

PipeCaptureBuffer::PipeCaptureBuffer(EventLoop &event_loop,
				     UniqueFileDescriptor _fd,
				     size_t capacity) noexcept
	:event(event_loop, BIND_THIS_METHOD(OnSocket), _fd.Release()),
	 buffer(capacity)
{
	event.ScheduleRead();
}

void
PipeCaptureBuffer::OnSocket(unsigned) noexcept
{
	FileDescriptor fd(event.GetFileDescriptor());

	auto w = buffer.Write();
	if (!w.empty()) {
		ssize_t nbytes = fd.Read(std::as_writable_bytes(w));
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
		std::byte discard[4096];
		if (fd.Read(discard) <= 0)
			Close();
	}
}
