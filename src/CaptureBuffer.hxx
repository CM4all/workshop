/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CAPTURE_BUFFER_HXX
#define CAPTURE_BUFFER_HXX

#include "util/WritableBuffer.hxx"

#include <memory>

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

    WritableBuffer<char> Write() {
        return { &data[size], capacity - size };
    }

    void Append(size_t n) {
        size += n;
    }

    WritableBuffer<char> GetData() {
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

#endif
