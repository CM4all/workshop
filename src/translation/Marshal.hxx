// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
