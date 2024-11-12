// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ProgressReader.hxx"
#include "io/UniqueFileDescriptor.hxx"

ProgressReader::ProgressReader(EventLoop &event_loop,
			       UniqueFileDescriptor _fd,
			       Callback _callback) noexcept
	:event(event_loop, BIND_THIS_METHOD(PipeReady), _fd.Release()),
	 callback(_callback)
{
	event.ScheduleRead();
}

void
ProgressReader::PipeReady(unsigned) noexcept
{
	char buffer[512];
	ssize_t nbytes, i;
	unsigned new_progress = 0, p;

	FileDescriptor fd(event.GetFileDescriptor());
	nbytes = fd.Read(std::as_writable_bytes(std::span{buffer}));
	if (nbytes <= 0) {
		event.Close();
		return;
	}

	for (i = 0; i < nbytes; ++i) {
		char ch = buffer[i];

		if (ch >= '0' && ch <= '9' &&
		    stdout_buffer.size() < stdout_buffer.max_size() - 1) {
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
