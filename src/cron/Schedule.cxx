// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Schedule.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "time/Convert.hxx"
#include "time/Math.hxx"
#include "util/StringAPI.hxx"
#include "util/StringStrip.hxx"
#include "util/Macros.hxx"

#include <exception> // for std::throw_with_nested()
#include <stdexcept>

#include <assert.h>
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
		if (StringIsEqualIgnoreCase(s, i->name, length)) {
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

	/* the numeric_limits check suppresses -Wtype-limits if MIN==0 */
	if constexpr (MIN > std::numeric_limits<decltype(value)>::min())
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
	std::chrono::seconds delay_range;
};

static constexpr CronSpecialSchedule cron_special_schedules[] = {
	{ "yearly", "0 0 1 1 *", std::chrono::hours(24 * 365) },
	{ "annually", "0 0 1 1 *", std::chrono::hours(24 * 365) },
	{ "monthly", "0 0 1 * *", std::chrono::hours(24 * 28) },
	{ "weekly", "0 0 * * 0", std::chrono::hours(24 * 7) },
	{ "daily", "0 0 * * *", std::chrono::hours(24) },
	{ "midnight", "0 0 * * *", std::chrono::hours(1) },
	{ "hourly", "0 * * * *", std::chrono::hours(1) },
};

static const CronSpecialSchedule *
TranslateSpecial(const char *s)
{
	for (const auto &i : cron_special_schedules)
		if (strcmp(s, i.special) == 0)
			return &i;

	return nullptr;
}

CronSchedule::CronSchedule(const char *const _s)
try {
	const char *s = _s;

	if (*s == '@') {
		++s;

		if (strcmp(s, "once") == 0) {
			assert(IsOnce());
			/* don't delay "@once" jobs; they shall be executed as
			   soon as they are added to the database */
			delay_range = std::chrono::seconds(0);
			return;
		}

		const auto *special = TranslateSpecial(s);
		if (special == nullptr)
			throw std::runtime_error("Unsupported 'special' cron schedule");

		s = special->regular;
		delay_range = special->delay_range;
	} else if (s[0] == '*' && s[1] == '/') {
		/* if a job shall be executed every N minutes, delay it
		   randomly up to those N minutes; this block is a simplified
		   parser just for calculating the delay_range */
		const char *t = s + 2;
		char *endptr;
		unsigned long value = strtoul(t, &endptr, 10);
		if (endptr > t && *endptr != ',')
			delay_range = std::chrono::minutes(value);
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
} catch (...) {
	std::throw_with_nested(FmtInvalidArgument("Failed to parse cron schedule {:?}", _s));
}

bool
CronSchedule::CheckDate(const struct tm &tm) const noexcept
{
	return days_of_month[tm.tm_mday] &&
		months[tm.tm_mon + 1] &&
		days_of_week[tm.tm_wday];
}

template<size_t MIN, size_t MAX>
[[gnu::const]]
static size_t
NextBit(const RangeBitSet<MIN, MAX> b, size_t pos) noexcept
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
		   std::chrono::system_clock::time_point now) const noexcept
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

	auto last = GmTime(_last);
	auto next = last;
	next.tm_sec = 0;

	if (!hours[next.tm_hour])
		/* last hour was invalid: recover by forcing a skip to the
		   next valid hour */
		last.tm_min = 60;

	next.tm_min = NextBit(minutes, last.tm_min);
	if (next.tm_min <= last.tm_min) {
		next.tm_hour = NextBit(hours, last.tm_hour);
		if (next.tm_hour <= last.tm_hour)
			IncrementDay(next);
	}

	while (!CheckDate(next))
		IncrementDay(next);

	return TimeGm(next);
}
