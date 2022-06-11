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

#include <memory>
#include <span>

/**
 * A buffer which helps capture up to 8 kB of data.
 */
class CaptureBuffer final {
	const size_t capacity;

	size_t size = 0;

	std::unique_ptr<char[]> data;

public:
	explicit CaptureBuffer(size_t _capacity)
		:capacity(_capacity),
		 data(new char[capacity]) {}

	bool IsFull() const noexcept {
		return size == capacity;
	}

	std::span<char> Write() {
		return { &data[size], capacity - size };
	}

	void Append(size_t n) {
		size += n;
	}

	std::span<char> GetData() {
		return {data.get(), size};
	}

	/**
	 * Convert all non-printable and non-ASCII characters except for
	 * CR/LF and tab to a space and null-terminate the string.  This
	 * modifies this object's buffer.
	 *
	 * @return the null-terminated ASCII string
	 */
	char *NormalizeASCII();
};
