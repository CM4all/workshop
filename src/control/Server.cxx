/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "Server.hxx"
#include "Handler.hxx"
#include "Crc.hxx"
#include "net/SocketAddress.hxx"
#include "util/ByteOrder.hxx"
#include "util/ConstBuffer.hxx"
#include "util/OffsetPointer.hxx"

#include <stdexcept>

ControlServer::ControlServer(EventLoop &event_loop, UniqueSocketDescriptor s,
			     ControlHandler &_handler)
	:handler(_handler), listener(event_loop, std::move(s), *this)
{
}

static ConstBuffer<void>
CheckDatagramHeader(ConstBuffer<void> p)
{
	if (p.size % 4 != 0)
		throw std::runtime_error("Odd control datagram size");

	const auto *header = (const WorkshopControlDatagramHeader *)p.data;
	if (p.size < sizeof(*header))
		throw std::runtime_error("Wrong control datagram size");

	p = {header + 1, p.size - sizeof(header)};

	if (FromBE32(header->magic) != WORKSHOP_CONTROL_MAGIC)
		throw std::runtime_error("Wrong magic");

	if (FromBE32(header->crc) != CalcWorkshopControlCrc(p))
		throw std::runtime_error("CRC error");

	return p;
}

static void
DecodeControlDatagram(ConstBuffer<void> p, ControlHandler &handler)
{
	p = CheckDatagramHeader(p);

	/* now decode all commands */

	while (!p.empty()) {
		const auto *header = (const WorkshopControlHeader *)p.data;
		if (p.size < sizeof(*header))
			throw std::runtime_error("Incomplete control command header");

		size_t payload_size = FromBE16(header->size);
		const auto command = (WorkshopControlCommand)FromBE16(header->command);

		p.data = header + 1;
		p.size -= sizeof(*header);

		const void *payload = p.data;
		if (p.size < payload_size)
			throw std::runtime_error("Partial control command payload");

		/* this command is ok, pass it to the callback */

		handler.OnControlPacket(command,
					payload_size > 0
					? ConstBuffer<void>(payload, payload_size)
					: nullptr);

		payload_size = ((payload_size + 3) | 3) - 3; /* apply padding */

		p.data = OffsetPointer(payload, payload_size);
		p.size -= payload_size;
	}
}

bool
ControlServer::OnUdpDatagram(const void *data, size_t length,
			     SocketAddress, int uid)
{
	if (uid != 0 && uid != (int)geteuid())
		/* ignore control packets from non-root users */
		return true;

	DecodeControlDatagram({data, length}, handler);
	return true;
}

void
ControlServer::OnUdpError(std::exception_ptr ep) noexcept
{
	handler.OnControlError(ep);
}
