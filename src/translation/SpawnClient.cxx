// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SpawnClient.hxx"
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

using std::string_view_literals::operator""sv;

static void
SendTranslateSpawn(SocketDescriptor s, const char *tag,
		   const char *plan_name,
		   const char *execute, const char *param)

{
	assert(execute != nullptr);

	if (strlen(execute) > 1024)
		throw std::runtime_error("Translation parameter too long");

	if (param != nullptr && strlen(param) > 4096)
		throw std::runtime_error("Translation parameter too long");

	TranslationMarshaller m;

	m.Write(TranslationCommand::BEGIN);

	m.Write(TranslationCommand::EXECUTE, execute);
	m.Write(TranslationCommand::PLAN, plan_name);
	m.Write(TranslationCommand::SERVICE, "workshop"sv);

	if (param != nullptr)
		m.Write(TranslationCommand::PARAM, param);

	if (tag != nullptr)
		m.Write(TranslationCommand::LISTENER_TAG, tag);

	m.Write(TranslationCommand::END);

	SendFull(s, m.Commit());
}

Co::Task<TranslateResponse>
TranslateSpawn(EventLoop &event_loop,
	       AllocatorPtr alloc, SocketDescriptor s,
	       const char *tag,
	       const char *plan_name, const char *execute, const char *param)
{
	SendTranslateSpawn(s, tag, plan_name, execute, param);
	co_await AwaitableSocketEvent(event_loop, s, SocketEvent::READ);
	co_return ReceiveTranslateResponse(alloc, s);
}
