/*
 * Copyright (C) 2008-2016 Max Kellermann <max@duempel.org>
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

#include "Request.hxx"
#include "Handler.hxx"
#include "Global.hxx"
#include "Version.hxx"
#include "event/SocketEvent.hxx"
#include "event/TimerEvent.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringUtil.hxx"
#include "util/StringView.hxx"

#include <curl/curl.h>

#include <assert.h>
#include <string.h>

CurlRequest::CurlRequest(CurlGlobal &_global, const char *url,
			 CurlResponseHandler &_handler)
	:global(_global), handler(_handler)
{
	error_buffer[0] = 0;

	easy.SetOption(CURLOPT_PRIVATE, (void *)this);
	easy.SetOption(CURLOPT_USERAGENT, "CM4all Workshop " VERSION);
	easy.SetOption(CURLOPT_HEADERFUNCTION, _HeaderFunction);
	easy.SetOption(CURLOPT_WRITEHEADER, this);
	easy.SetOption(CURLOPT_WRITEFUNCTION, WriteFunction);
	easy.SetOption(CURLOPT_WRITEDATA, this);
	easy.SetOption(CURLOPT_FAILONERROR, 1l);
	easy.SetOption(CURLOPT_ERRORBUFFER, error_buffer);
	easy.SetOption(CURLOPT_NOPROGRESS, 1l);
	easy.SetOption(CURLOPT_NOSIGNAL, 1l);
	easy.SetOption(CURLOPT_CONNECTTIMEOUT, 10l);
	easy.SetOption(CURLOPT_URL, url);

	global.Add(*this);
}

CurlRequest::~CurlRequest()
{
	FreeEasy();
}

void
CurlRequest::FreeEasy()
{
	if (!easy)
		return;

	global.Remove(*this);
	easy = nullptr;
}

void
CurlRequest::Resume()
{
	curl_easy_pause(easy.Get(), CURLPAUSE_CONT);

	if (IsCurlOlderThan(0x072000))
		/* libcurl older than 7.32.0 does not update
		   its sockets after curl_easy_pause(); force
		   libcurl to do it now */
		global.ResumeSockets();

	global.InvalidateSockets();
}

void
CurlRequest::Done(CURLcode result)
{
	FreeEasy();

	try {
		if (result != CURLE_OK) {
			StripRight(error_buffer);
			const char *msg = error_buffer;
			if (*msg == 0)
				msg = curl_easy_strerror(result);
			throw FormatRuntimeError("CURL failed: %s", msg);
		}
	} catch (...) {
		state = State::CLOSED;
		handler.OnError(std::current_exception());
		return;
	}

	state = State::CLOSED;
	handler.OnEnd();
}

inline void
CurlRequest::HeaderReceived(const char *name, std::string &&value)
{
	headers.emplace(name, std::move(value));
}

inline void
CurlRequest::HeadersFinished()
{
	assert(state == State::HEADERS);
	state = State::BODY;

	long status = 0;
	curl_easy_getinfo(easy.Get(), CURLINFO_RESPONSE_CODE, &status);

	try {
		handler.OnHeaders(status, std::move(headers));
	} catch (...) {
		state = State::CLOSED;
		handler.OnError(std::current_exception());
	}
}

inline void
CurlRequest::HeaderFunction(StringView s)
{
	if (state > State::HEADERS)
		return;

	const char *header = s.data;
	const char *end = StripRight(header, header + s.size);
	if (end == header) {
		HeadersFinished();
		return;
	}

	char name[64];

	const char *value = s.Find(':');
	if (value == nullptr || (size_t)(value - header) >= sizeof(name))
		return;

	memcpy(name, header, value - header);
	name[value - header] = 0;

	/* skip the colon */

	++value;

	/* strip the value */

	value = StripLeft(value, end);
	end = StripRight(value, end);

	HeaderReceived(name, std::string(value, end));
}

size_t
CurlRequest::_HeaderFunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
	CurlRequest &c = *(CurlRequest *)stream;

	size *= nmemb;

	c.HeaderFunction({(const char *)ptr, size});
	return size;
}

inline size_t
CurlRequest::DataReceived(const void *ptr, size_t received_size)
{
	assert(received_size > 0);

	try {
		handler.OnData({ptr, received_size});
		return received_size;
	} catch (Pause) {
		return CURL_WRITEFUNC_PAUSE;
	} catch (...) {
		state = State::CLOSED;
		handler.OnError(std::current_exception());
		return 0;
	}
}

size_t
CurlRequest::WriteFunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
	CurlRequest &c = *(CurlRequest *)stream;

	size *= nmemb;
	if (size == 0)
		return 0;

	return c.DataReceived(ptr, size);
}
