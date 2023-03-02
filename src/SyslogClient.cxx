// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SyslogClient.hxx"
#include "net/RConnectSocket.hxx"
#include "net/AddressInfo.hxx"
#include "system/Error.hxx"
#include "io/Iovec.hxx"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>

SyslogClient::SyslogClient(const char *host_and_port,
			   const char *_me, const char *_ident,
			   int _facility)
	:SyslogClient(ResolveConnectDatagramSocket(host_and_port, 514),
		      _me, _ident, _facility)
{
}

static constexpr struct iovec
MakeIovec(const std::string_view value) noexcept
{
	return MakeIovec(std::span{value});
}

int
SyslogClient::Log(int priority, std::string_view msg) noexcept
{
	static const char space = ' ';
	static const char newline = '\n';
	static const char colon[] = ": ";

	std::array<char, 16> code;
	struct iovec iovec[] = {
		MakeIovec(std::span{code.data(), std::size_t(0)}),
		MakeIovec(me),
		MakeIovecT(space),
		MakeIovec(ident),
		MakeIovec(colon),
		MakeIovec(msg),
		MakeIovecT(newline),
	};
	ssize_t nbytes;

	assert(fd.IsDefined());
	assert(priority >= 0 && priority < 8);

	snprintf(code.data(), sizeof(code.size()), "<%d>",
		 facility * 8 + priority);
	iovec[0].iov_len = strlen(code.data());

	nbytes = writev(fd.Get(), iovec, std::size(iovec));
	if (nbytes < 0)
		return errno;

	if (nbytes == 0)
		return -1;

	return 0;
}
