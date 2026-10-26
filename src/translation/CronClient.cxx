// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

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

static constexpr std::size_t MAX_PARTITION_NAME = 256;
static constexpr std::size_t MAX_LISTENER_TAG = 256;
static constexpr std::size_t MAX_USER = 256;
static constexpr std::size_t MAX_URI = 2048;
static constexpr std::size_t MAX_PARAM = 4096;

/* the largest possible request must fit into the
   TranslationMarshaller buffer (8 KiB); there are 7 packets
   (BEGIN, CRON, LISTENER_TAG, USER, URI, PARAM, END) */
static_assert(7 * sizeof(TranslationHeader) +
	      MAX_PARTITION_NAME + MAX_LISTENER_TAG + MAX_USER +
	      MAX_URI + MAX_PARAM <= 8_KiB);

static void
SendTranslateCron(SocketDescriptor s,
		  std::string_view partition_name,
		  const char *listener_tag,
		  const char *user, const char *uri, const char *param)
{
	assert(user != nullptr);

	if (partition_name.size() > MAX_PARTITION_NAME)
		throw std::runtime_error("Partition name too long");

	if (listener_tag != nullptr && strlen(listener_tag) > MAX_LISTENER_TAG)
		throw std::runtime_error("Listener tag too long");

	if (strlen(user) > MAX_USER)
		throw std::runtime_error("User name too long");

	if (uri != nullptr && strlen(uri) > MAX_URI)
		throw std::runtime_error("URI too long");

	if (param != nullptr && strlen(param) > MAX_PARAM)
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
