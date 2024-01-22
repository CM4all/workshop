// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CronClient.hxx"
#include "Marshal.hxx"
#include "Send.hxx"
#include "Receive.hxx"
#include "translation/Protocol.hxx"
#include "translation/Response.hxx"
#include "AllocatorPtr.hxx"
#include "event/AwaitableSocketEvent.hxx"
#include "net/SocketDescriptor.hxx"
#include "co/Task.hxx"

#include <stdexcept>

static void
SendTranslateCron(SocketDescriptor s,
		  std::string_view partition_name,
		  const char *listener_tag,
		  const char *user, const char *uri, const char *param)
{
	assert(user != nullptr);

	if (strlen(user) > 256)
		throw std::runtime_error("User name too long");

	if (param != nullptr && strlen(param) > 4096)
		throw std::runtime_error("Translation parameter too long");

	TranslationMarshaller m;

	m.Write(TranslationCommand::BEGIN);

	m.Write(TranslationCommand::CRON, partition_name);

	if (listener_tag != nullptr)
		m.Write(TranslationCommand::LISTENER_TAG, listener_tag);

	m.Write(TranslationCommand::USER, user);
	if (uri != nullptr)
		m.Write(TranslationCommand::URI, uri);
	if (param != nullptr)
		m.Write(TranslationCommand::PARAM, param);
	m.Write(TranslationCommand::END);

	SendFull(s, m.Commit());
}

Co::Task<TranslateResponse>
TranslateCron(EventLoop &event_loop,
	      AllocatorPtr alloc, SocketDescriptor s,
	      std::string_view partition_name,
	      const char *listener_tag,
	      const char *user, const char *uri,
	      const char *param)
{
	SendTranslateCron(s, partition_name, listener_tag, user, uri, param);
	co_await AwaitableSocketEvent(event_loop, s, SocketEvent::READ);
	co_return ReceiveTranslateResponse(alloc, s);
}
