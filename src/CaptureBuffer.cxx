/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "CaptureBuffer.hxx"
#include "util/CharUtil.hxx"

#include <algorithm>

static constexpr bool
IsAllowedNonPrintableChar(char ch)
{
    return ch == '\r' || ch == '\n' || ch == '\t';
}

static constexpr bool
IsDisallowedChar(char ch)
{
    return !IsPrintableASCII(ch) && !IsAllowedNonPrintableChar(ch);
}

char *
CaptureBuffer::NormalizeASCII()
{
    if (size == capacity)
        /* crop the last character to make room for the null
           terminator */
        size = capacity - 1;

    std::replace_if(data.get(), std::next(data.get(), size),
                    IsDisallowedChar, ' ');
    data[size] = 0;
    return data.get();
}
