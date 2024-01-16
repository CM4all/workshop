// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CronGlue.hxx"
#include "CronClient.hxx"
#include "translation/Response.hxx"
#include "AllocatorPtr.hxx"
#include "net/ConnectSocket.hxx"
#include "net/SocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"

TranslateResponse
TranslateCron(AllocatorPtr alloc, SocketAddress address,
	      std::string_view partition_name,
	      const char *listener_tag,
	      const char *user, const char *uri, const char *param)
{
	auto s = CreateConnectSocket(address, SOCK_STREAM);
	s.SetBlocking();

	return TranslateCron(alloc, s, partition_name, listener_tag,
			     user, uri, param);
}
