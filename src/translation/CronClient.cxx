/*
 * Copyright 2006-2022 CM4all GmbH
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

#include "CronClient.hxx"
#include "Marshal.hxx"
#include "Receive.hxx"
#include "translation/Protocol.hxx"
#include "translation/Response.hxx"
#include "AllocatorPtr.hxx"
#include "system/Error.hxx"
#include "net/SocketDescriptor.hxx"

#include <stdexcept>

#include <sys/socket.h>

static void
SendFull(SocketDescriptor s, std::span<const std::byte> buffer)
{
	ssize_t nbytes = send(s.Get(), buffer.data(), buffer.size(),
			      MSG_NOSIGNAL);
	if (nbytes < 0)
		throw MakeErrno("send() to translation server failed");

	if (size_t(nbytes) != buffer.size())
		throw std::runtime_error("Short send() to translation server");
}

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
