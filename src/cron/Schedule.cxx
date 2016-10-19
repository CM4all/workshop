/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Schedule.hxx"
#include "time/Convert.hxx"
#include "time/Math.hxx"
#include "util/StringUtil.hxx"

#include <stdexcept>

#include <assert.h>
#include <stdlib.h>
#include <time.h>

template<size_t MIN, size_t MAX>
static void
MakeRangeBitSet(RangeBitSet<MIN, MAX> &b,
                 unsigned first, unsigned last, unsigned step)
{
    for (unsigned i = first; i <= last; i += step)
        b[i] = true;
}

template<size_t MIN, size_t MAX>
static void
ParseNumericRangeBitSet(RangeBitSet<MIN, MAX> &b, const char *&schedule)
{
    unsigned first, last;

    if (*schedule == '*') {
        ++schedule;
        first = MIN;
        last = MAX;
    } else {
        char *endptr;
        first = last = strtoul(schedule, &endptr, 10);
        if (endptr == schedule)
            throw std::runtime_error("Failed to parse number");

        if (first < MIN)
            throw std::runtime_error("Number is too large");

        if (first > MAX)
            throw std::runtime_error("Number is too large");

        schedule = endptr;

        if (*schedule == '-') {
            ++schedule;
            last = strtoul(schedule, &endptr, 10);
            if (endptr == schedule)
                throw std::runtime_error("Failed to parse number");

            if (last > MAX)
                throw std::runtime_error("Number is too large");

            if (last < first)
                throw std::runtime_error("Malformed range");

            schedule = endptr;
        } else
            last = first;
    }

    unsigned step = 1;
    if (*schedule == '/') {
        ++schedule;

        char *endptr;
        step = strtoul(schedule, &endptr, 10);
        if (endptr == schedule)
            throw std::runtime_error("Failed to parse number");

        if (step > MAX)
            throw std::runtime_error("Number is too large");

        schedule = endptr;
    }

    MakeRangeBitSet(b, first, last, step);
}

template<size_t MIN, size_t MAX>
static void
ParseNumericBitSet(RangeBitSet<MIN, MAX> &b, const char *&schedule)
{
    schedule = StripLeft(schedule);

    while (true) {
        ParseNumericRangeBitSet<MIN, MAX>(b, schedule);
        assert(b.count() > 0);
        if (*schedule != ',')
            break;

        ++schedule;
    }
}

CronSchedule::CronSchedule(const char *s)
{
    ParseNumericBitSet(minutes, s);
    ParseNumericBitSet(hours, s);
    ParseNumericBitSet(days_of_month, s);
    ParseNumericBitSet(months, s);

    RangeBitSet<0, 7> _days_of_week;
    ParseNumericBitSet(_days_of_week, s);

    for (unsigned i = 0; i < days_of_week.size(); ++i)
        days_of_week[i] = _days_of_week[i];

    if (_days_of_week[days_of_week.size()])
        days_of_week[0] = true;

    s = StripLeft(s);
    if (*s != 0)
        throw std::runtime_error("Garbage at end of schedule");
}

bool
CronSchedule::CheckDate(const struct tm &tm) const
{
    return days_of_month[tm.tm_mday] &&
        months[tm.tm_mon + 1] &&
        days_of_week[tm.tm_wday];
}

bool
CronSchedule::CheckTime(const struct tm &tm) const
{
    return minutes[tm.tm_sec] && hours[tm.tm_hour];
}

bool
CronSchedule::Check(const struct tm &tm) const
{
    return CheckDate(tm) && CheckTime(tm);
}

bool
CronSchedule::Check(std::chrono::system_clock::time_point t) const
{
    return Check(LocalTime(t));
}

template<size_t MIN, size_t MAX>
gcc_const
static size_t
NextBit(const RangeBitSet<MIN, MAX> b, size_t pos)
{
    for (size_t i = pos + 1; i <= MAX; ++i)
        if (b[i])
            return i;

    for (size_t i = MIN; i < pos; ++i)
        if (b[i])
            return i;

    return pos;
}

std::chrono::system_clock::time_point
CronSchedule::Next(std::chrono::system_clock::time_point _last) const
{
    auto last = LocalTime(_last);
    auto next = last;
    next.tm_sec = 0;

    next.tm_min = NextBit(minutes, last.tm_min);
    if (next.tm_min <= last.tm_min) {
        /* TODO: what about daylight saving transitions? */
        next.tm_hour = NextBit(hours, last.tm_hour);
        if (next.tm_hour <= last.tm_hour)
            IncrementDay(next);
    }

    while (!CheckDate(next))
        IncrementDay(next);

    return MakeTime(next);
}