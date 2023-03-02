// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "PipeCaptureBuffer.hxx"
#include "net/SocketDescriptor.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <string>
#include <string_view>

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
	void OnLine(std::string_view line) noexcept;
	void SendLines(bool flush) noexcept;
};
