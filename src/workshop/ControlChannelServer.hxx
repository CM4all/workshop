// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/net/UdpHandler.hxx"
#include "event/net/UdpListener.hxx"

#include <string>
#include <vector>

class UniqueSocketDescriptor;
class WorkshopControlChannelHandler;

/**
 * The control channel through which Workshop job processes can send
 * commands to Workshop.
 */
class WorkshopControlChannelServer final : UdpHandler {
	UdpListener socket;

	WorkshopControlChannelHandler &handler;

public:
	WorkshopControlChannelServer(EventLoop &_event_loop,
				     UniqueSocketDescriptor &&_socket,
				     WorkshopControlChannelHandler &_handler) noexcept;

	bool ReceiveAll() {
		return socket.ReceiveAll();
	}

private:
	void InvokeTemporaryError(const char *msg) noexcept;

	bool OnControl(std::vector<std::string> &&args) noexcept;

	/* virtual methods from UdpHandler */
	bool OnUdpDatagram(std::span<const std::byte> payload,
			   std::span<UniqueFileDescriptor> fds,
			   SocketAddress address, int uid) override;
	void OnUdpError(std::exception_ptr e) noexcept override;
};
