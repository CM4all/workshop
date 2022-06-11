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
#include "translation/Parser.hxx"
#include "translation/Protocol.hxx"
#include "translation/Response.hxx"
#include "AllocatorPtr.hxx"
#include "system/Error.hxx"
#include "util/StaticFifoBuffer.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringView.hxx"

#include <stdexcept>

#include <string.h>
#include <sys/socket.h>

static void
WriteHeader(void *&p, TranslationCommand command, size_t size)
{
	TranslationHeader header;
	header.length = (uint16_t)size;
	header.command = command;

	p = mempcpy(p, &header, sizeof(header));
}

static void
WritePacket(void *&p, TranslationCommand cmd)
{
	WriteHeader(p, cmd, 0);
}

static void
WritePacket(void *&p, TranslationCommand cmd,
	    ConstBuffer<void> payload)
{
	WriteHeader(p, cmd, payload.size);
	p = mempcpy(p, payload.data, payload.size);
}

static void
WritePacket(void *&p, TranslationCommand cmd,
	    StringView payload)
{
	WritePacket(p, cmd, payload.ToVoid());
}

static void
SendFull(int fd, ConstBuffer<void> buffer)
{
	ssize_t nbytes = send(fd, buffer.data, buffer.size, MSG_NOSIGNAL);
	if (nbytes < 0)
		throw MakeErrno("send() to translation server failed");

	if (size_t(nbytes) != buffer.size)
		throw std::runtime_error("Short send() to translation server");
}

static void
SendTranslateCron(int fd, const char *partition_name, const char *listener_tag,
		  const char *user, const char *uri, const char *param)
{
	assert(user != nullptr);

	if (strlen(user) > 256)
		throw std::runtime_error("User name too long");

	if (param != nullptr && strlen(param) > 4096)
		throw std::runtime_error("Translation parameter too long");

	char buffer[8192];
	void *p = buffer;

	WritePacket(p, TranslationCommand::BEGIN);

	size_t partition_name_size = partition_name != nullptr
		? strlen(partition_name)
		: 0;
	WritePacket(p, TranslationCommand::CRON,
		    StringView(partition_name, partition_name_size));

	if (listener_tag != nullptr)
		WritePacket(p, TranslationCommand::LISTENER_TAG, listener_tag);

	WritePacket(p, TranslationCommand::USER, user);
	if (uri != nullptr)
		WritePacket(p, TranslationCommand::URI, uri);
	if (param != nullptr)
		WritePacket(p, TranslationCommand::PARAM, param);
	WritePacket(p, TranslationCommand::END);

	const size_t size = (char *)p - buffer;
	SendFull(fd, {buffer, size});
}

static TranslateResponse
ReceiveResponse(AllocatorPtr alloc, int fd)
{
	TranslateResponse response;
	TranslateParser parser(alloc, response);

	StaticFifoBuffer<uint8_t, 8192> buffer;

	while (true) {
		auto w = buffer.Write();
		if (w.empty())
			throw std::runtime_error("Translation receive buffer is full");

		ssize_t nbytes = recv(fd, w.data(), w.size(), MSG_NOSIGNAL);
		if (nbytes < 0)
			throw MakeErrno("recv() from translation server failed");

		if (nbytes == 0)
			throw std::runtime_error("Translation server hung up");

		buffer.Append(nbytes);

		while (true) {
			auto r = buffer.Read();
			if (r.empty())
				break;

			size_t consumed = parser.Feed(r.data(), r.size());
			if (consumed == 0)
				break;

			buffer.Consume(consumed);

			auto result = parser.Process();
			switch (result) {
			case TranslateParser::Result::MORE:
				break;

			case TranslateParser::Result::DONE:
				if (!buffer.empty())
					throw std::runtime_error("Excessive data from translation server");

				return response;
			}
		}
	}
}

TranslateResponse
TranslateCron(AllocatorPtr alloc, int fd,
	      const char *partition_name, const char *listener_tag,
	      const char *user, const char *uri,
	      const char *param)
{
	SendTranslateCron(fd, partition_name, listener_tag, user, uri, param);
	return ReceiveResponse(alloc, fd);
}
