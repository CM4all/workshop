/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef RANGE_BIT_SET_HXX
#define RANGE_BIT_SET_HXX

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

#endif
