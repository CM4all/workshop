// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Protocol.hxx"

#include <cstddef>
#include <exception>
#include <span>

class SocketAddress;

class ControlHandler {
public:
	virtual void OnControlPacket(WorkshopControlCommand command,
				     std::span<const std::byte> payload) = 0;

	virtual void OnControlError(std::exception_ptr ep) noexcept = 0;
};
