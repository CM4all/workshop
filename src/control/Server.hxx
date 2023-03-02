// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/net/UdpHandler.hxx"
#include "event/net/UdpListener.hxx"

class ControlHandler;
class SocketAddress;
class UniqueSocketDescriptor;
class UdpListener;
class EventLoop;
struct SocketConfig;

/**
 * Server side part of the "control" protocol.
 */
class ControlServer final : UdpHandler {
	ControlHandler &handler;

	UdpListener listener;

public:
	ControlServer(EventLoop &event_loop, UniqueSocketDescriptor s,
		      ControlHandler &_handler);

private:
	/* virtual methods from class UdpHandler */
	bool OnUdpDatagram(std::span<const std::byte> payload,
			   std::span<UniqueFileDescriptor> fds,
			   SocketAddress address, int uid) override;
	void OnUdpError(std::exception_ptr ep) noexcept override;
};
