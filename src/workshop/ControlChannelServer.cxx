// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ControlChannelServer.hxx"
#include "ControlChannelHandler.hxx"
#include "net/SocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/ScmRightsBuilder.hxx"
#include "net/SendMessage.hxx"
#include "io/Iovec.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/Exception.hxx"
#include "util/IterableSplitString.hxx"
#include "util/SpanCast.hxx"
#include "util/StringSplit.hxx"
#include "version.h"

#include <stdexcept>

using std::string_view_literals::operator""sv;

WorkshopControlChannelServer::WorkshopControlChannelServer(EventLoop &_event_loop,
							   UniqueSocketDescriptor &&_socket,
							   WorkshopControlChannelHandler &_handler) noexcept
	:socket(_event_loop, std::move(_socket), *this),
	 handler(_handler) {}

void
WorkshopControlChannelServer::InvokeTemporaryError(const char *msg) noexcept
{
	handler.OnControlTemporaryError(std::make_exception_ptr(std::runtime_error(msg)));
}

bool
WorkshopControlChannelServer::OnControl(std::vector<std::string> &&args) noexcept
{
	const auto &cmd = args.front();

	if (cmd == "progress"sv) {
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

		handler.OnControlProgress(progress);
		return true;
	} else if (cmd == "setenv"sv) {
		if (args.size() != 2) {
			InvokeTemporaryError("malformed 'setenv' command on control channel");
			return true;
		}

		handler.OnControlSetEnv(args[1].c_str());
		return true;
	} else if (cmd == "again"sv) {
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

		handler.OnControlAgain(d);
		return true;
	} else if (cmd == "version"sv) {
		constexpr std::string_view payload = "version " VERSION;
		socket.GetSocket().Write(payload.data(), payload.size());
		return true;
	} else if (cmd == "spawn"sv) {
		if (args.size() < 2 || args.size() > 3) {
			InvokeTemporaryError("malformed 'spawn' command on control channel");
			return true;
		}

		const char *token = args[1].c_str();
		const char *param = args.size() >= 3
			? args[2].c_str()
			: nullptr;

		try {
			auto pidfd = handler.OnControlSpawn(token, param);
			assert(pidfd.IsDefined());

			const struct iovec v[]{MakeIovec(AsBytes("ok"sv))};
			MessageHeader msg{std::span{v}};
			ScmRightsBuilder<1> b(msg);

			b.push_back(pidfd.Get());

			b.Finish(msg);
			SendMessage(socket.GetSocket(), msg, MSG_NOSIGNAL);
		} catch (...) {
			const auto msg = GetFullMessage(std::current_exception());
			InvokeTemporaryError(msg.c_str());

			const struct iovec v[] = {
				MakeIovec(AsBytes("error "sv)),
				MakeIovec(AsBytes(msg)),
			};

			try {
				SendMessage(socket.GetSocket(), MessageHeader{v},
					    MSG_NOSIGNAL);
			} catch (...) {
				socket.Close();
				handler.OnControlPermanentError(std::current_exception());
			}
		}

		return true;
	} else {
		InvokeTemporaryError("unknown command on control channel");
		return true;
	}
}

[[gnu::pure]]
static std::string_view
FirstLine(std::string_view s) noexcept
{
	return Split(s, '\n').first;
}

[[gnu::pure]]
static std::vector<std::string>
SplitArgs(std::string_view s) noexcept
{
	std::vector<std::string> result;
	for (const std::string_view i : IterableSplitString(s, ' '))
		result.emplace_back(i);
	return result;
}

bool
WorkshopControlChannelServer::OnUdpDatagram(std::span<const std::byte> payload,
					    std::span<UniqueFileDescriptor>,
					    SocketAddress, int)
{
	if (payload.empty()) {
		handler.OnControlClosed();
		return false;
	}

	return OnControl(SplitArgs(FirstLine(ToStringView(payload))));
}

void
WorkshopControlChannelServer::OnUdpError(std::exception_ptr e) noexcept
{
	handler.OnControlPermanentError(e);
}
