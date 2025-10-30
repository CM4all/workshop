// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/PipeLineReader.hxx"
#include "config.h"

#include <string>

class LogBridge final : PipeLineReaderHandler {
	const std::string plan_name, job_id;

	PipeLineReader reader;
#ifdef HAVE_LIBSYSTEMD
	bool enable_journal = false;
#endif // HAVE_LIBSYSTEMD

	std::string buffer;
	size_t max_buffer_size = 0;

public:
	LogBridge(EventLoop &event_loop,
		  std::string_view _plan_name, std::string_view _job_id,
		  UniqueFileDescriptor read_pipe_fd) noexcept;
	~LogBridge() noexcept;

	void EnableBuffer(size_t max_size) noexcept {
		max_buffer_size = max_size;
	}

	const char *GetBuffer() const noexcept {
		return max_buffer_size > 0
			? buffer.c_str()
			: nullptr;
	}

	void EnableJournal() noexcept {
#ifdef HAVE_LIBSYSTEMD
		enable_journal = true;
#endif
	}

	void Flush() noexcept {
		if (reader.IsDefined())
			reader.Flush();
	}

private:
	bool OnPipeLine(std::span<char> line) noexcept override;
	void OnPipeEnd() noexcept override {}
};
