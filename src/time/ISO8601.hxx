/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef TIME_ISO8601_HXX
#define TIME_ISO8601_HXX

#include <string>
#include <chrono>

struct tm;

std::string
FormatISO8601(const struct tm &tm);

std::string
FormatISO8601(std::chrono::system_clock::time_point tp);

std::chrono::system_clock::time_point
ParseISO8601(const char *s);

#endif
