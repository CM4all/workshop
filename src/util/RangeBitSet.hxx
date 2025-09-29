// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <bitset>

/**
 * A variant of std::bitset which has a lower and upper bound.  The
 * "MIN" offset is applied to all internal bits.
 */
template<std::size_t MIN, std::size_t MAX>
class RangeBitSet {
	using BitSet = std::bitset<MAX - MIN + 1>;

	BitSet b;

public:
	constexpr bool operator==(const RangeBitSet<MIN, MAX> &other) const noexcept = default;

	constexpr std::size_t size() const noexcept {
		return b.size();
	}

	constexpr std::size_t count() const noexcept {
		return b.count();
	}

	constexpr std::size_t all() const noexcept {
		return b.all();
	}

	constexpr std::size_t any() const noexcept {
		return b.any();
	}

	constexpr std::size_t none() const noexcept {
		return b.none();
	}

	constexpr bool operator[](std::size_t pos) const noexcept {
		return b[pos - MIN];
	}

	constexpr auto &set(std::size_t pos, bool value=true) noexcept {
		b.set(pos - MIN, value);
		return *this;
	}

	constexpr auto &set() noexcept {
		b.set();
		return *this;
	}
};
