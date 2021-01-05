/*
 * Copyright 2017-2021 CM4all GmbH
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
