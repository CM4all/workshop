/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Math.hxx"
#include "Calendar.hxx"

#include <time.h>

void
IncrementDay(struct tm &tm)
{
    const unsigned max_day = DaysInMonth(tm.tm_mon + 1, tm.tm_year + 1900);

    ++tm.tm_mday;

    if ((unsigned)tm.tm_mday > max_day) {
        /* roll over to next month */
        tm.tm_mday = 1;
        ++tm.tm_mon;
        if (tm.tm_mon >= 12) {
            /* roll over to next year */
            tm.tm_mon = 0;
            ++tm.tm_year;
        }
    }

    ++tm.tm_wday;
    if (tm.tm_wday >= 7)
        tm.tm_wday = 0;
}
