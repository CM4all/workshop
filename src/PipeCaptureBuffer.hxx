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

#include "CaptureBuffer.hxx"
#include "event/PipeEvent.hxx"

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

	WritableBuffer<char> GetData() noexcept {
		return buffer.GetData();
	}

	char *NormalizeASCII() noexcept {
		return buffer.NormalizeASCII();
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
