// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "CaptureBuffer.hxx"
#include "event/PipeEvent.hxx"
#include "util/AllocatedString.hxx"

class UniqueFileDescriptor;

/**
 * Capture up to 8 kB of data from a pipe asynchronously.  This is
 * useful to capture the output of a child process.
 */
class PipeCaptureBuffer {
	PipeEvent event;

	CaptureBuffer buffer;

public:
	explicit PipeCaptureBuffer(EventLoop &event_loop,
				   UniqueFileDescriptor _fd,
				   size_t capacity) noexcept;
	virtual ~PipeCaptureBuffer() noexcept {
		Close();
	}

	bool IsFull() const noexcept {
		return buffer.IsFull();
	}

	std::span<char> GetData() noexcept {
		return buffer.GetData();
	}

	AllocatedString NormalizeASCII() && noexcept {
		return std::move(buffer).NormalizeASCII();
	}

protected:
	/**
	 * This method is called whenever new data was appended.
	 */
	virtual void OnAppend() noexcept {}

	/**
	 * This method is called at the end of the pipe, or when the
	 * buffer has become full.
	 */
	virtual void OnEnd() noexcept {}

private:
	void Close() noexcept {
		event.Close();
	}

	void OnSocket(unsigned events) noexcept;
};
