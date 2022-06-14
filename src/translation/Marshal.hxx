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

#pragma once

#include "translation/Protocol.hxx"
#include "util/SpanCast.hxx"

#include <algorithm>
#include <array>
#include <span>
#include <string_view>
#include <utility>

class TranslationMarshaller {
	std::array<std::byte, 8192> buffer;

	std::byte *tail = buffer.data();

public:
	void Write(std::span<const std::byte> src) noexcept {
		tail = std::copy(src.begin(), src.end(), tail);
	}

	void WriteHeader(TranslationCommand command, size_t size) noexcept {
		TranslationHeader header;
		header.length = (uint16_t)size;
		header.command = command;

		Write(std::as_bytes(std::span{&header, 1}));
	}

	void Write(TranslationCommand command,
		   std::span<const std::byte> payload={}) noexcept {
		WriteHeader(command, payload.size());
		Write(payload);
	}

	void Write(TranslationCommand command,
		   std::string_view payload) noexcept {
		Write(command, AsBytes(payload));
	}

	std::span<const std::byte> Commit() const noexcept {
		return std::span{buffer}.first(tail - buffer.data());
	}
};
