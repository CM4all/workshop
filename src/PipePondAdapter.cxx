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

#include "PipePondAdapter.hxx"
#include "net/log/Send.hxx"
#include "net/log/Datagram.hxx"
#include "util/StringView.hxx"
#include "util/PrintException.hxx"

#include <string.h>

void
PipePondAdapter::OnLine(StringView line) noexcept
{
	if (line.empty())
		return;

	Net::Log::Datagram d;
	if (!site.empty())
		d.site = site.c_str();

	d.message = line;

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

	auto r = GetData();
	r.skip_front(position);

	while (!r.empty()) {
		char *newline = (char *)memchr(r.data, '\n', r.size);
		if (newline != nullptr) {
			const StringView line((const char *)r.data, newline);
			position += line.size + 1;
			r.skip_front(line.size + 1);
			OnLine(line);

			/* check the socket again - it may have been closed by
			   OnLine() if an error occurred */
			if (!pond_socket.IsDefined())
				break;
		} else if (flush) {
			const StringView line((const char *)r.data, r.size);
			position += r.size;
			OnLine(line);
			break;
		} else
			break;
	}
}
