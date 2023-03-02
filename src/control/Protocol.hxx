// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <stdint.h>

/*

  Workshop listens on a datagram socket for commands.  Each datagram
  begins with the 32 bit "magic", followed by a CRC32 of all command
  packets, followed by one or more command packets.  Each command
  packet begins with a header and a variable-length payload.  The
  payloads are padded to the next multiple of 4 bytes.

*/

static constexpr uint16_t WORKSHOP_CONTROL_DEFAULT_PORT = 5484;

/**
 * This magic number precedes every UDP packet.
 */
static const uint32_t WORKSHOP_CONTROL_MAGIC = 0x63046102;

struct WorkshopControlDatagramHeader {
	uint32_t magic;
	uint32_t crc;
};

enum class WorkshopControlCommand : uint16_t {
	NOP = 0,

	/**
	 * Set the logger verbosity.  The payload is one byte: 0 means
	 * quiet, 1 is the default, and bigger values make the daemon more
	 * verbose.
	 */
	VERBOSE = 1,

	/**
	 * Disable all queues, i.e. do not accept any new jobs.
	 */
	DISABLE_QUEUE = 2,

	/**
	 * Re-enable all queues, i.e. resume accepting new jobs.
	 */
	ENABLE_QUEUE = 3,
};

struct WorkshopControlHeader {
	uint16_t size;
	uint16_t command;
};
