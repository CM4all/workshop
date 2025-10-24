// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LogBridge.hxx"
#include "util/SpanCast.hxx"

#include <fmt/core.h>

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-journal.h>
#endif // HAVE_LIBSYSTEMD

LogBridge::LogBridge(EventLoop &event_loop,
		     std::string_view _plan_name,
		     std::string_view _job_id,
		     UniqueFileDescriptor read_pipe_fd)
	:plan_name(_plan_name), job_id(_job_id),
	 reader(event_loop, std::move(read_pipe_fd), *this)
{
}

LogBridge::~LogBridge() = default;

bool
LogBridge::OnPipeLine(std::span<char> line) noexcept
{
	// TODO: strip non-ASCII characters

	if (max_buffer_size > 0 && buffer.length() < max_buffer_size - 1) {
		buffer.append(line.data(),
			      std::min(line.size(), max_buffer_size - 1 - buffer.length()));
		buffer.push_back('\n');
	}

#ifdef HAVE_LIBSYSTEMD
	if (enable_journal)
		sd_journal_send("MESSAGE=%.*s", int(line.size()), line.data(),
				"WORKSHOP_PLAN=%s", plan_name.c_str(),
				"WORKSHOP_JOB=%s", job_id.c_str(),
				nullptr);
#else
	constexpr bool enable_journal = false;
#endif // HAVE_LIBSYSTEMD

	if (max_buffer_size == 0 && !enable_journal)
		fmt::print(stderr, "[{}:{}] {}\n", plan_name, job_id,
			   ToStringView(line));

	return true;
}
