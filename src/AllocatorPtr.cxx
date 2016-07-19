/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "AllocatorPtr.hxx"
#include "util/StringView.hxx"

#include <algorithm>

StringView
AllocatorPtr::Dup(StringView src)
{
    if (src.IsNull())
        return nullptr;

    if (src.IsEmpty())
        return "";

    return {(const char *)Dup(src.data, src.size), src.size};
}

const char *
AllocatorPtr::DupZ(StringView src)
{
    if (src.IsNull())
        return nullptr;

    if (src.IsEmpty())
        return "";

    char *p = NewArray<char>(src.size + 1);
    *std::copy_n(src.data, src.size, p) = 0;
    return p;
}

