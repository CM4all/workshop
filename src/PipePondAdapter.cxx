// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PipePondAdapter.hxx"
#include "net/log/Send.hxx"
#include "net/log/Datagram.hxx"
#include "util/PrintException.hxx"

#include <string.h>

void
PipePondAdapter::OnLine(std::string_view line) noexcept
{
	if (line.empty())
		return;

	Net::Log::Datagram d{
		.message = line,
		.type = Net::Log::Type::JOB,
	};

	/* truncate long lines */
	d.TruncateMessage(1024);

	if (!site.empty())
		d.site = site.c_str();

	try {
		Net::Log::Send(pond_socket, d);
	} catch (...) {
		PrintException(std::current_exception());
		pond_socket.SetUndefined();
	}
}

void
PipePondAdapter::SendLines(bool flush) noexcept
{
	if (!pond_socket.IsDefined())
		return;

	auto r = GetData().subspan(position);

	while (!r.empty()) {
		char *newline = (char *)memchr(r.data(), '\n', r.size());
		if (newline != nullptr) {
			const std::string_view line{(const char *)r.data(), newline};
			position += line.size() + 1;
			r = r.subspan(line.size() + 1);
			OnLine(line);

			/* check the socket again - it may have been closed by
			   OnLine() if an error occurred */
			if (!pond_socket.IsDefined())
				break;
		} else if (flush) {
			const std::string_view line{(const char *)r.data(), r.size()};
			position += r.size();
			OnLine(line);
			break;
		} else
			break;
	}
}
