// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <span>

class SocketDescriptor;

void
SendFull(SocketDescriptor s, std::span<const std::byte> buffer);
