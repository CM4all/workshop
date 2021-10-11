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

#include "ControlChannelServer.hxx"
#include "ControlChannelListener.hxx"
#include "net/SocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/IterableSplitString.hxx"
#include "util/StringView.hxx"
#include "util/WritableBuffer.hxx"
#include "version.h"

#include <stdexcept>

WorkshopControlChannelServer::WorkshopControlChannelServer(EventLoop &_event_loop,
							   UniqueSocketDescriptor &&_socket,
							   WorkshopControlChannelListener &_listener) noexcept
	:socket(_event_loop, std::move(_socket), *this),
	 listener(_listener) {}

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

[[gnu::pure]]
static StringView
FirstLine(StringView s) noexcept
{
	return s.Split('\n').first;
}

[[gnu::pure]]
static std::vector<std::string>
SplitArgs(StringView s) noexcept
{
	std::vector<std::string> result;
	for (const std::string_view i : IterableSplitString(s, ' '))
		result.emplace_back(i);
	return result;
}

bool
WorkshopControlChannelServer::OnUdpDatagram(ConstBuffer<void> _payload,
					    WritableBuffer<UniqueFileDescriptor>,
					    SocketAddress, int)
{
	if (_payload.empty()) {
		listener.OnControlClosed();
		return false;
	}

	const StringView payload{ConstBuffer<char>::FromVoid(_payload)};
	return OnControl(SplitArgs(FirstLine(payload)));
}

void
WorkshopControlChannelServer::OnUdpError(std::exception_ptr e) noexcept
{
	listener.OnControlPermanentError(e);
}
