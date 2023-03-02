// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
