// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LogBridge.hxx"
#include "SyslogClient.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "util/SpanCast.hxx"

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
	const auto ident = FmtBuffer<256>("{}[{}]", plan_name, job_id);

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
		fmt::print(stderr, "[{}:{}] {}\n", plan_name, job_id,
			   ToStringView(line));

	return true;
}
