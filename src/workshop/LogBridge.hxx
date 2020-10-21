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
	bool OnPipeLine(WritableBuffer<char> line) noexcept override;
	void OnPipeEnd() noexcept override {}
};
