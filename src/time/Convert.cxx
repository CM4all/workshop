/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Convert.hxx"

#include <stdexcept>

#include <time.h>

struct tm
GmTime(std::chrono::system_clock::time_point tp)
{
    const time_t t = std::chrono::system_clock::to_time_t(tp);
    struct tm buffer, *tm = gmtime_r(&t, &buffer);
    if (tm == nullptr)
        throw std::runtime_error("gmtime_r() failed");

    return *tm;
}

struct tm
LocalTime(std::chrono::system_clock::time_point tp)
{
    const time_t t = std::chrono::system_clock::to_time_t(tp);
    struct tm buffer, *tm = localtime_r(&t, &buffer);
    if (tm == nullptr)
        throw std::runtime_error("localtime_r() failed");

    return *tm;
}

std::chrono::system_clock::time_point
TimeGm(struct tm &tm)
{
    return std::chrono::system_clock::from_time_t(timegm(&tm));
}

std::chrono::system_clock::time_point
MakeTime(struct tm &tm)
{
    return std::chrono::system_clock::from_time_t(mktime(&tm));
}
