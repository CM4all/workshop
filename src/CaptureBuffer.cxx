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
    if (length == data.size())
        /* crop the last character to make room for the null
           terminator */
        length = data.size() - 1;

    std::replace_if(data.begin(), std::next(data.begin(), length),
                    IsDisallowedChar, ' ');
    data[length] = 0;
    return &data.front();
}
