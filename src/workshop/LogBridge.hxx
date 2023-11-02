// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/PipeLineReader.hxx"
#include "config.h"

#include <memory>
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
		  const char *_plan_name, const char *_job_id,
		  UniqueFileDescriptor read_pipe_fd);
	~LogBridge();

	void EnableBuffer(size_t max_size) {
		max_buffer_size = max_size;
	}

	const char *GetBuffer() const {
		return max_buffer_size > 0
			? buffer.c_str()
			: nullptr;
	}

	void EnableJournal() {
#ifdef HAVE_LIBSYSTEMD
		enable_journal = true;
#endif
	}

	void Flush() noexcept {
		reader.Flush();
	}

private:
	bool OnPipeLine(std::span<char> line) noexcept override;
	void OnPipeEnd() noexcept override {}
};
