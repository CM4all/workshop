/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "ISO8601.hxx"
#include "Convert.hxx"

#include <stdexcept>

#include <time.h>

std::string
FormatISO8601(const struct tm &tm)
{
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%FT%TZ", &tm);
    return buffer;
}

std::string
FormatISO8601(std::chrono::system_clock::time_point tp)
{
    return FormatISO8601(GmTime(tp));
}

std::chrono::system_clock::time_point
ParseISO8601(const char *s)
{
    struct tm tm;
    const char *end = strptime(s, "%FT%TZ", &tm);
    if (end == nullptr || *end != 0)
        throw std::runtime_error("Failed to parse ISO8601");

    return TimeGm(tm);
}

