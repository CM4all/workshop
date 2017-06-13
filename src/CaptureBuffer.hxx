/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CAPTURE_BUFFER_HXX
#define CAPTURE_BUFFER_HXX

#include "util/WritableBuffer.hxx"

#include <array>

/**
 * A buffer which helps capture up to 8 kB of data.
 */
class CaptureBuffer final {
    size_t size = 0;
    std::array<char, 8192> data;

public:
    WritableBuffer<char> Write() {
        return { &data[size], data.size() - size };
    }

    void Append(size_t n) {
        size += n;
    }

    WritableBuffer<char> GetData() {
        return {&data.front(), size};
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

#endif
