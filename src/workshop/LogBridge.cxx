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

#include "LogBridge.hxx"
#include "SyslogClient.hxx"
#include "util/StringView.hxx"

#include <systemd/sd-journal.h>

LogBridge::LogBridge(EventLoop &event_loop,
		     const char *_plan_name, const char *_job_id,
		     UniqueFileDescriptor read_pipe_fd)
	:plan_name(_plan_name), job_id(_job_id),
	 reader(event_loop, std::move(read_pipe_fd), *this)
{
}

LogBridge::~LogBridge() = default;

void
LogBridge::CreateSyslog(const char *host_and_port,
			const char *me,
			int facility)
{
	char ident[256];
	snprintf(ident, sizeof(ident), "%s[%s]",
		 plan_name.c_str(), job_id.c_str());

	syslog.reset(new SyslogClient(host_and_port, me, ident, facility));
}

bool
LogBridge::OnPipeLine(std::span<char> line) noexcept
{
	// TODO: strip non-ASCII characters

	if (max_buffer_size > 0 && buffer.length() < max_buffer_size - 1) {
		buffer.append(line.data(),
			      std::min(line.size(), max_buffer_size - 1 - buffer.length()));
		buffer.push_back('\n');
	}

	if (syslog)
		syslog->Log(6, {line.data(), line.size()});

	if (enable_journal)
		sd_journal_send("MESSAGE=%.*s", int(line.size()), line.data(),
				"WORKSHOP_PLAN=%s", plan_name.c_str(),
				"WORKSHOP_JOB=%s", job_id.c_str(),
				nullptr);

	if (max_buffer_size == 0 && !syslog && !enable_journal)
		fprintf(stderr, "[%s:%s] %.*s\n", plan_name.c_str(), job_id.c_str(),
			int(line.size()), line.data());

	return true;
}
