// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/PipeLineReader.hxx"

#include <memory>
#include <string>

class SyslogClient;

class LogBridge final : PipeLineReaderHandler {
	const std::string plan_name, job_id;

	PipeLineReader reader;
	std::unique_ptr<SyslogClient> syslog;
	bool enable_journal = false;

	std::string buffer;
	size_t max_buffer_size = 0;

public:
	LogBridge(EventLoop &event_loop,
		  const char *_plan_name, const char *_job_id,
		  UniqueFileDescriptor read_pipe_fd);
	~LogBridge();

	void CreateSyslog(const char *host_and_port,
			  const char *me,
			  int facility);

	void EnableBuffer(size_t max_size) {
		max_buffer_size = max_size;
	}

	const char *GetBuffer() const {
		return max_buffer_size > 0
			? buffer.c_str()
			: nullptr;
	}

	void EnableJournal() {
		enable_journal = true;
	}

	void Flush() noexcept {
		reader.Flush();
	}

private:
	bool OnPipeLine(std::span<char> line) noexcept override;
	void OnPipeEnd() noexcept override {}
};
