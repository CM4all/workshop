// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <bitset>

/**
 * A variant of std::bitset which has a lower and upper bound.  The
 * "MIN" offset is applied to all internal bits.
 */
template<size_t MIN, size_t MAX>
class RangeBitSet {
	typedef std::bitset<MAX - MIN + 1> BitSet;

	BitSet b;

public:
	bool operator==(const RangeBitSet<MIN, MAX> &other) const noexcept = default;

	constexpr size_t size() const noexcept {
		return b.size();
	}

	constexpr size_t count() const noexcept {
		return b.count();
	}

	constexpr size_t all() const noexcept {
		return b.all();
	}

	constexpr size_t any() const noexcept {
		return b.any();
	}

	constexpr size_t none() const noexcept {
		return b.none();
	}

	constexpr bool operator[](size_t pos) const noexcept {
		return b[pos - MIN];
	}

	typename BitSet::reference operator[](size_t pos) noexcept {
		return b[pos - MIN];
	}
};
