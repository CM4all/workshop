// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
