// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Server.hxx"
#include "Handler.hxx"
#include "util/CRC32.hxx"
#include "net/SocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/ByteOrder.hxx"

#include <stdexcept>

ControlServer::ControlServer(EventLoop &event_loop, UniqueSocketDescriptor s,
			     ControlHandler &_handler)
	:handler(_handler), listener(event_loop, std::move(s), *this)
{
}

static std::span<const std::byte>
CheckDatagramHeader(std::span<const std::byte> p)
{
	if (p.size() % 4 != 0)
		throw std::runtime_error("Odd control datagram size");

	const auto *header = (const WorkshopControlDatagramHeader *)(const void *)p.data();
	if (p.size() < sizeof(*header))
		throw std::runtime_error("Wrong control datagram size");

	p = p.subspan(sizeof(*header));

	if (FromBE32(header->magic) != WORKSHOP_CONTROL_MAGIC)
		throw std::runtime_error("Wrong magic");

	if (FromBE32(header->crc) != CRC32(p))
		throw std::runtime_error("CRC error");

	return p;
}

static void
DecodeControlDatagram(std::span<const std::byte> p, ControlHandler &handler)
{
	p = CheckDatagramHeader(p);

	/* now decode all commands */

	while (!p.empty()) {
		const auto *header = (const WorkshopControlHeader *)(const void *)p.data();
		if (p.size() < sizeof(*header))
			throw std::runtime_error("Incomplete control command header");

		size_t payload_size = FromBE16(header->size);
		const auto command = (WorkshopControlCommand)FromBE16(header->command);

		p = p.subspan(sizeof(*header));

		if (p.size() < payload_size)
			throw std::runtime_error("Partial control command payload");

		/* this command is ok, pass it to the callback */

		handler.OnControlPacket(command, p.first(payload_size));

		payload_size = ((payload_size + 3) | 3) - 3; /* apply padding */

		p = p.subspan(payload_size);
	}
}

bool
ControlServer::OnUdpDatagram(std::span<const std::byte> payload,
			     std::span<UniqueFileDescriptor>,
			     SocketAddress, int uid)
{
	if (uid != 0 && uid != (int)geteuid())
		/* ignore control packets from non-root users */
		return true;

	DecodeControlDatagram(payload, handler);
	return true;
}

void
ControlServer::OnUdpError(std::exception_ptr ep) noexcept
{
	handler.OnControlError(ep);
}
