// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/UniqueSocketDescriptor.hxx"

#include <string>
#include <string_view>

class SyslogClient {
	UniqueSocketDescriptor fd;
	const std::string me, ident;
	const int facility;

public:
	SyslogClient(UniqueSocketDescriptor _fd,
		     const char *_me, const char *_ident, int _facility)
		:fd(std::move(_fd)), me(_me), ident(_ident), facility(_facility) {}

	/**
	 * Throws std::runtime_error on error.
	 */
	SyslogClient(const char *host_and_port,
		     const char *_me, const char *_ident, int _facility);

	SyslogClient(SyslogClient &&src) = default;

	int Log(int priority, std::string_view msg) noexcept;
};
