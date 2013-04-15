/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SNOWBALL_BINARY_VALUE_HXX
#define SNOWBALL_BINARY_VALUE_HXX

#include <inline/compiler.h>

#include <cstddef>

struct BinaryValue {
    const void *value;
    size_t size;

    BinaryValue() = default;
    constexpr BinaryValue(const void *_value, size_t _size)
        :value(_value), size(_size) {}

    gcc_pure
    bool ToBool() const {
        return size == 1 && value != nullptr && *(const bool *)value;
    }
};

#endif
