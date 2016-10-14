/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef TIME_MATH_HXX
#define TIME_MATH_HXX

struct tm;

/**
 * Calculate the next day, keeping month/year wraparounds and leap
 * days in mind.  Keeps the tm_wday attribute updated, but not other
 * derived attributes such as tm_yday, and ignores day light saving
 * transitions.
 */
void
IncrementDay(struct tm &tm);

#endif
