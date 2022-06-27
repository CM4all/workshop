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

#include "Receive.hxx"
#include "translation/Parser.hxx"
#include "translation/Protocol.hxx"
#include "translation/Response.hxx"
#include "AllocatorPtr.hxx"
#include "system/Error.hxx"
#include "net/SocketDescriptor.hxx"
#include "util/StaticFifoBuffer.hxx"

#include <stdexcept>

#include <sys/socket.h>

TranslateResponse
ReceiveTranslateResponse(AllocatorPtr alloc, SocketDescriptor s)
{
	TranslateResponse response;
	TranslateParser parser(alloc, response);

	StaticFifoBuffer<std::byte, 8192> buffer;

	while (true) {
		auto w = buffer.Write();
		if (w.empty())
			throw std::runtime_error("Translation receive buffer is full");

		ssize_t nbytes = recv(s.Get(), w.data(), w.size(), MSG_NOSIGNAL);
		if (nbytes < 0)
			throw MakeErrno("recv() from translation server failed");

		if (nbytes == 0)
			throw std::runtime_error("Translation server hung up");

		buffer.Append(nbytes);

		while (true) {
			auto r = buffer.Read();
			if (r.empty())
				break;

			size_t consumed = parser.Feed(r);
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
