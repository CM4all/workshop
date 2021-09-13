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

#pragma once

#include "event/net/UdpHandler.hxx"
#include "event/net/UdpListener.hxx"

#include <string>
#include <vector>

class UniqueSocketDescriptor;
class WorkshopControlChannelListener;

/**
 * The control channel through which Workshop job processes can send
 * commands to Workshop.
 */
class WorkshopControlChannelServer final : UdpHandler {
	UdpListener socket;

	WorkshopControlChannelListener &listener;

public:
	WorkshopControlChannelServer(EventLoop &_event_loop,
				     UniqueSocketDescriptor &&_socket,
				     WorkshopControlChannelListener &_listener) noexcept
		:socket(_event_loop, std::move(_socket), *this),
		 listener(_listener) {}

	bool ReceiveAll() {
		return socket.ReceiveAll();
	}

private:
	void InvokeTemporaryError(const char *msg) noexcept;

	bool OnControl(std::vector<std::string> &&args) noexcept;

	/* virtual methods from UdpHandler */
	bool OnUdpDatagram(ConstBuffer<void> payload,
			   WritableBuffer<UniqueFileDescriptor> fds,
			   SocketAddress address, int uid) override;
	void OnUdpError(std::exception_ptr e) noexcept override;
};
