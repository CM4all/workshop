// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/net/UdpHandler.hxx"
#include "event/net/UdpListener.hxx"
#include "co/InvokeTask.hxx"

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

	Co::InvokeTask task;

public:
	WorkshopControlChannelServer(EventLoop &_event_loop,
				     UniqueSocketDescriptor &&_socket,
				     WorkshopControlChannelHandler &_handler) noexcept;

	bool ReceiveAll() {
		return socket.ReceiveAll();
	}

private:
	void InvokeTemporaryError(const char *msg) noexcept;

	void StartTask(Co::InvokeTask &&_task) noexcept;
	void OnTaskFinished(std::exception_ptr &&error) noexcept;

	Co::InvokeTask OnSpawn(std::vector<std::string> args);
	bool OnControl(std::vector<std::string> &&args) noexcept;

	/* virtual methods from UdpHandler */
	bool OnUdpDatagram(std::span<const std::byte> payload,
			   std::span<UniqueFileDescriptor> fds,
			   SocketAddress address, int uid) override;
	void OnUdpError(std::exception_ptr &&error) noexcept override;
};
