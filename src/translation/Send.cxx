// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Send.hxx"
#include "net/SocketDescriptor.hxx"
#include "net/SocketError.hxx"

#include <sys/socket.h>

class SocketDescriptor;

void
SendFull(SocketDescriptor s, std::span<const std::byte> buffer)
{
	ssize_t nbytes = send(s.Get(), buffer.data(), buffer.size(),
			      MSG_NOSIGNAL);
	if (nbytes < 0)
		throw MakeSocketError("send() to translation server failed");

	if (size_t(nbytes) != buffer.size())
		throw std::runtime_error("Short send() to translation server");
}

