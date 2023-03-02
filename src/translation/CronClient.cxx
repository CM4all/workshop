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
#include "net/SocketDescriptor.hxx"

#include <stdexcept>

static void
SendTranslateCron(SocketDescriptor s, const char *partition_name, const char *listener_tag,
		  const char *user, const char *uri, const char *param)
{
	assert(user != nullptr);

	if (strlen(user) > 256)
		throw std::runtime_error("User name too long");

	if (param != nullptr && strlen(param) > 4096)
		throw std::runtime_error("Translation parameter too long");

	TranslationMarshaller m;

	m.Write(TranslationCommand::BEGIN);

	size_t partition_name_size = partition_name != nullptr
		? strlen(partition_name)
		: 0;
	m.Write(TranslationCommand::CRON,
		std::string_view{partition_name, partition_name_size});

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

TranslateResponse
TranslateCron(AllocatorPtr alloc, SocketDescriptor s,
	      const char *partition_name, const char *listener_tag,
	      const char *user, const char *uri,
	      const char *param)
{
	SendTranslateCron(s, partition_name, listener_tag, user, uri, param);
	return ReceiveTranslateResponse(alloc, s);
}
