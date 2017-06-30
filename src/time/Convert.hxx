/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef TIME_CONVERT_HXX
#define TIME_CONVERT_HXX

#include "util/Compiler.h"

#include <chrono>

/**
 * Convert a UTC-based time point to a UTC-based "struct tm".
 */
gcc_const
struct tm
GmTime(std::chrono::system_clock::time_point tp);

/**
 * Convert a UTC-based time point to a local "struct tm".
 */
gcc_const
struct tm
LocalTime(std::chrono::system_clock::time_point tp);

/**
 * Convert a UTC-based "struct tm" to a UTC-based time point.
 */
gcc_pure
std::chrono::system_clock::time_point
TimeGm(struct tm &tm);

/**
 * Convert a local "struct tm" to a UTC-based time point.
 */
gcc_pure
std::chrono::system_clock::time_point
MakeTime(struct tm &tm);

#endif
