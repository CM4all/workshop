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

#include "ControlChannelServer.hxx"
#include "ControlChannelListener.hxx"
#include "net/SocketAddress.hxx"
#include "util/IterableSplitString.hxx"
#include "util/StringView.hxx"

#include <stdexcept>

void
WorkshopControlChannelServer::InvokeTemporaryError(const char *msg) noexcept
{
	listener.OnControlTemporaryError(std::make_exception_ptr(std::runtime_error(msg)));
}

bool
WorkshopControlChannelServer::OnControl(std::vector<std::string> &&args) noexcept
{
	const auto &cmd = args.front();

	if (cmd == "progress") {
		if (args.size() != 2) {
			InvokeTemporaryError("malformed progress command on control channel");
			return true;
		}

		char *endptr;
		auto progress = strtoul(args[1].c_str(), &endptr, 10);
		if (*endptr != 0 || progress > 100) {
			InvokeTemporaryError("malformed progress command on control channel");
			return true;
		}

		listener.OnControlProgress(progress);
		return true;
	} else if (cmd == "setenv") {
		if (args.size() != 2) {
			InvokeTemporaryError("malformed 'setenv' command on control channel");
			return true;
		}

		listener.OnControlSetEnv(args[1].c_str());
		return true;
	} else if (cmd == "again") {
		if (args.size() > 2) {
			InvokeTemporaryError("malformed 'again' command on control channel");
			return true;
		}

		std::chrono::seconds d(0);
		if (args.size() >= 2) {
			const char *s = args[1].c_str();
			char *endptr;
			auto value = strtoull(s, &endptr, 10);
			if (endptr == s || *endptr != 0 || value > 3600 * 24) {
				InvokeTemporaryError("malformed 'again' parameter on control channel");
				return true;
			}

			d = std::chrono::seconds(value);
		}

		listener.OnControlAgain(d);
		return true;
	} else if (cmd == "version") {
		StringView payload("version " VERSION);
		socket.GetSocket().Write(payload.data, payload.size);
		return true;
	} else {
		InvokeTemporaryError("unknown command on control channel");
		return true;
	}
}

gcc_pure
static StringView
FirstLine(StringView s) noexcept
{
	const char *newline = s.Find('\n');
	if (newline != nullptr)
		s.SetEnd(newline);
	return s;
}

gcc_pure
static std::vector<std::string>
SplitArgs(StringView s) noexcept
{
	std::vector<std::string> result;
	for (StringView i : IterableSplitString(s, ' '))
		result.emplace_back(i.data, i.size);
	return result;
}

bool
WorkshopControlChannelServer::OnUdpDatagram(const void *data, size_t length,
					    SocketAddress, int)
{
	if (length == 0) {
		listener.OnControlClosed();
		return false;
	}

	const StringView payload((const char *)data, length);
	return OnControl(SplitArgs(FirstLine(payload)));
}

void
WorkshopControlChannelServer::OnUdpError(std::exception_ptr e) noexcept
{
	listener.OnControlPermanentError(e);
}
