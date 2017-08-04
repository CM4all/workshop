/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Schedule.hxx"
#include "time/Convert.hxx"
#include "time/Math.hxx"
#include "util/StringUtil.hxx"
#include "util/Macros.hxx"

#include <stdexcept>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

struct CronSymbol {
    const char *name;
    unsigned value;
};

static constexpr CronSymbol month_names[] = {
    { "jan", 1 },
    { "feb", 2 },
    { "mar", 3 },
    { "apr", 4 },
    { "may", 5 },
    { "jun", 6 },
    { "jul", 7 },
    { "aug", 8 },
    { "sep", 9 },
    { "oct", 10 },
    { "nov", 11 },
    { "dec", 12 },
};

static constexpr CronSymbol days_of_week_names[] = {
    { "mon", 1 },
    { "tue", 2 },
    { "wed", 3 },
    { "thu", 4 },
    { "fri", 5 },
    { "sat", 6 },
    { "sun", 7 },
};

static_assert(ARRAY_SIZE(month_names) == 12, "Wrong number of months");

static const CronSymbol *
LookupCronSymbol(const char *&s, const CronSymbol dictionary[])
{
    assert(s != nullptr);
    assert(dictionary != nullptr);

    for (const auto *i = dictionary; i->name != nullptr; ++i) {
        size_t length = strlen(i->name);
        if (strncmp(s, i->name, length) == 0) {
            s += length;
            return i;
        }
    }

    return nullptr;
}

template<size_t MIN, size_t MAX>
static void
MakeRangeBitSet(RangeBitSet<MIN, MAX> &b,
                 unsigned first, unsigned last, unsigned step)
{
    for (unsigned i = first; i <= last; i += step)
        b[i] = true;
}

template<unsigned long MIN, unsigned long MAX>
static unsigned
ParseNumber(const char *&s,
            const CronSymbol dictionary[])
{
    char *endptr;
    unsigned long value = strtoul(s, &endptr, 10);
    if (endptr == s) {
        if (dictionary != nullptr) {
            auto *i = LookupCronSymbol(s, dictionary);
            if (i != nullptr)
                return i->value;
        }

        throw std::runtime_error("Failed to parse number");
    }

    if (value < MIN)
        throw std::runtime_error("Number is too small");

    if (value > MAX)
        throw std::runtime_error("Number is too large");

    s = endptr;
    return value;
}

template<size_t MIN, size_t MAX>
static void
ParseNumericRangeBitSet(RangeBitSet<MIN, MAX> &b, const char *&schedule,
                        const CronSymbol dictionary[])
{
    unsigned first, last;

    if (*schedule == '*') {
        ++schedule;
        first = MIN;
        last = MAX;
    } else {
        first = last = ParseNumber<MIN, MAX>(schedule, dictionary);

        if (*schedule == '-') {
            ++schedule;

            last = ParseNumber<MIN, MAX>(schedule, dictionary);
            if (last < first)
                throw std::runtime_error("Malformed range");
        } else
            last = first;
    }

    unsigned step = 1;
    if (*schedule == '/') {
        ++schedule;

        step = ParseNumber<1, MAX>(schedule, dictionary);
    }

    MakeRangeBitSet(b, first, last, step);
}

template<size_t MIN, size_t MAX>
static void
ParseNumericBitSet(RangeBitSet<MIN, MAX> &b, const char *&schedule,
                   const CronSymbol dictionary[]=nullptr)
{
    schedule = StripLeft(schedule);

    while (true) {
        ParseNumericRangeBitSet<MIN, MAX>(b, schedule, dictionary);
        assert(b.count() > 0);
        if (*schedule != ',')
            break;

        ++schedule;
    }
}

struct CronSpecialSchedule {
    const char *special;
    const char *regular;
};

static constexpr CronSpecialSchedule cron_special_schedules[] = {
    { "yearly", "0 0 1 1 *" },
    { "annually", "0 0 1 1 *" },
    { "monthly", "0 0 1 * *" },
    { "weekly", "0 0 * * 0" },
    { "daily", "0 0 * * *" },
    { "midnight", "0 0 * * *" },
    { "hourly", "0 * * * *" },
};

static const char *
TranslateSpecial(const char *s)
{
        for (const auto &i : cron_special_schedules)
            if (strcmp(s, i.special) == 0)
                return i.regular;

        return nullptr;
}

CronSchedule::CronSchedule(const char *s)
{
    if (*s == '@') {
        ++s;

        if (strcmp(s, "once") == 0) {
            assert(IsOnce());
            return;
        }

        const char *regular = TranslateSpecial(s);
        if (regular == nullptr)
            throw std::runtime_error("Unsupported 'special' cron schedule");

        s = regular;
    }

    ParseNumericBitSet(minutes, s);
    ParseNumericBitSet(hours, s);
    ParseNumericBitSet(days_of_month, s);
    ParseNumericBitSet(months, s, month_names);

    RangeBitSet<0, 7> _days_of_week;
    ParseNumericBitSet(_days_of_week, s, days_of_week_names);

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
CronSchedule::Next(std::chrono::system_clock::time_point _last,
                   std::chrono::system_clock::time_point now) const
{
    if (IsOnce()) {
        /* a "@once" job: execute it now or never again */
        if (_last == std::chrono::system_clock::time_point::min())
            /* was never run: do it now */
            return now;
        else
            /* already run at least once: never again */
            return std::chrono::system_clock::time_point::max();
    }

    if (_last == std::chrono::system_clock::time_point::min())
        _last = now - std::chrono::minutes(1);

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
