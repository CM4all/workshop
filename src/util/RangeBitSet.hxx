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
	bool operator==(const RangeBitSet<MIN, MAX> &other) const {
		return b == other.b;
	}

	bool operator!=(const RangeBitSet<MIN, MAX> &other) const {
		return !(*this == other);
	}

	constexpr size_t size() const {
		return b.size();
	}

	constexpr size_t count() const {
		return b.count();
	}

	constexpr size_t all() const {
		return b.all();
	}

	constexpr size_t any() const {
		return b.any();
	}

	constexpr size_t none() const {
		return b.none();
	}

	constexpr bool operator[](size_t pos) const {
		return b[pos - MIN];
	}

	typename BitSet::reference operator[](size_t pos) {
		return b[pos - MIN];
	}
};
