// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "CronGlue.hxx"
#include "CronClient.hxx"
#include "translation/Response.hxx"
#include "AllocatorPtr.hxx"
#include "net/ConnectSocket.hxx"
#include "net/SocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "co/Task.hxx"

Co::Task<TranslateResponse>
TranslateCron(EventLoop &event_loop,
	      AllocatorPtr alloc, SocketAddress address,
	      std::string_view partition_name,
	      const char *listener_tag,
	      const char *user, const char *uri, const char *param)
{
	auto s = CreateConnectSocket(address, SOCK_STREAM);
	s.SetBlocking();

	co_return co_await TranslateCron(event_loop, alloc, s,
					 partition_name, listener_tag,
					 user, uri, param);
}
