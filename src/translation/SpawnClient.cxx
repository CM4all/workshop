// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

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

static constexpr std::size_t MAX_TAG = 256;
static constexpr std::size_t MAX_PLAN_NAME = 256;
static constexpr std::size_t MAX_EXECUTE = 1024;
static constexpr std::size_t MAX_PARAM = 4096;

/**
 * The size of the TranslationMarshaller buffer.
 */
static constexpr std::size_t MAX_REQUEST = 8_KiB;

/* the largest possible request without APPEND packets must fit into
   the TranslationMarshaller buffer; there are 7 packets (BEGIN,
   EXECUTE, PLAN, SERVICE, PARAM, LISTENER_TAG, END) */
static_assert(7 * sizeof(TranslationHeader) +
	      MAX_TAG + MAX_PLAN_NAME + MAX_EXECUTE + "workshop"sv.size() +
	      MAX_PARAM <= MAX_REQUEST);

static constexpr std::size_t
PacketSize(std::string_view payload) noexcept
{
	return sizeof(TranslationHeader) + payload.size();
}

static void
SendTranslateSpawn(SocketDescriptor s, const char *tag,
		   const char *plan_name,
		   const char *execute, const char *param,
		   const std::forward_list<std::string> &args)

{
	assert(execute != nullptr);

	if (tag != nullptr && strlen(tag) > MAX_TAG)
		throw std::runtime_error("Listener tag too long");

	if (strlen(plan_name) > MAX_PLAN_NAME)
		throw std::runtime_error("Plan name too long");

	if (strlen(execute) > MAX_EXECUTE)
		throw std::runtime_error("Translation parameter too long");

	if (param != nullptr && strlen(param) > MAX_PARAM)
		throw std::runtime_error("Translation parameter too long");

	/* the size of all packets except for APPEND */
	std::size_t size = 2 * PacketSize({}) /* BEGIN, END */ +
		PacketSize(execute) + PacketSize(plan_name) +
		PacketSize("workshop"sv);

	if (param != nullptr)
		size += PacketSize(param);

	if (tag != nullptr)
		size += PacketSize(tag);

	for (const auto &i : args) {
		size += PacketSize(i);
		if (size > MAX_REQUEST)
			throw std::runtime_error("Too many arguments");
	}

	TranslationMarshaller m;

	m.Write(TranslationCommand::BEGIN);

	m.Write(TranslationCommand::EXECUTE, execute);
	m.Write(TranslationCommand::PLAN, plan_name);
	m.Write(TranslationCommand::SERVICE, "workshop"sv);

	if (param != nullptr)
		m.Write(TranslationCommand::PARAM, param);

	if (tag != nullptr)
		m.Write(TranslationCommand::LISTENER_TAG, tag);

	for (const auto &i : args)
		m.Write(TranslationCommand::APPEND, i);

	m.Write(TranslationCommand::END);

	SendFull(s, m.Commit());
}

Co::Task<TranslateResponse>
TranslateSpawn(EventLoop &event_loop,
	       AllocatorPtr alloc, SocketDescriptor s,
	       const char *tag,
	       const char *plan_name, const char *execute, const char *param,
	       const std::forward_list<std::string> &args)
{
	SendTranslateSpawn(s, tag, plan_name, execute, param, args);
	co_await AwaitableSocketEvent(event_loop, s, SocketEvent::READ);
	co_return ReceiveTranslateResponse(alloc, s);
}
