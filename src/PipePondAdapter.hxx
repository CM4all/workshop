/*
 * Copyright 2006-2021 CM4all GmbH
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

#pragma once

#include "PipeCaptureBuffer.hxx"
#include "net/SocketDescriptor.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <string>

struct StringView;

/**
 * A derivation from #PipeCaptureBuffer which adds Pond logging.
 */
class PipePondAdapter : public PipeCaptureBuffer {
	SocketDescriptor pond_socket;

	const std::string site;

	size_t position = 0;

public:
	template<typename S>
	explicit PipePondAdapter(EventLoop &event_loop,
				 UniqueFileDescriptor &&_fd,
				 size_t capacity,
				 SocketDescriptor _pond_socket,
				 S &&_site)
		:PipeCaptureBuffer(event_loop, std::move(_fd), capacity),
		 pond_socket(_pond_socket), site(std::forward<S>(_site)) {}

protected:
	void OnAppend() noexcept override {
		SendLines(false);
	}

	void OnEnd() noexcept override {
		SendLines(true);
	}

private:
	void OnLine(StringView line) noexcept;
	void SendLines(bool flush) noexcept;
};
