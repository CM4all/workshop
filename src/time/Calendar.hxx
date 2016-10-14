/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CALENDAR_HXX
#define CALENDAR_HXX

#include <inline/compiler.h>

gcc_const
static inline bool
IsLeapYear(unsigned y)
{
    y += 1900;
    return (y % 4) == 0 && ((y % 100) != 0 || (y % 400) == 0);
}

gcc_const
static inline unsigned
DaysInFebruary(unsigned year)
{
  return IsLeapYear(year) ? 29 : 28;
}

gcc_const
static inline unsigned
DaysInMonth(unsigned month, unsigned year)
{
  if (month == 4 || month == 6 || month == 9 || month == 11)
    return 30;
  else if (month != 2)
    return 31;
  else
    return DaysInFebruary(year);
}

#endif
