// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "CalculateNextRun.hxx"
#include "Schedule.hxx"
#include "pg/Connection.hxx"
#include "pg/Interval.hxx"
#include "pg/Timestamp.hxx"
#include "io/Logger.hxx"
#include "time/Convert.hxx"
#include "util/StringBuffer.hxx"

#include <random>

#include <assert.h>

namespace {

/**
 * An exception class that is thrown when another node has already
 * scheduled the job we were about to schedule.  This class exists
 * only so this kind of error has a different log verbosity.
 */
struct LostRace {};

}

static bool
IsSameInterval(const char *a, std::chrono::seconds b) noexcept
{
	try {
		return a != nullptr && Pg::ParseIntervalS(a) == b;
	} catch (...) {
		return false;
	}
}

static int64_t
RandomInt64(int64_t range)
{
	if (range <= 1)
		/* avoid integer underflow; if the range is zero, then no
		   delay is wanted */
		return 0;

	// TODO: use global generator
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint64_t> dis(0, range - 1);

	return dis(gen);
}

void
InitCalculateNextRun(Pg::Connection &db)
{
	db.Prepare("make_random_delay", R"SQL(
UPDATE cronjobs
 SET delay=$3::interval, delay_range=$4::interval
 WHERE id=$1 AND schedule=$2 AND enabled AND next_run IS NULL
)SQL",
		   4);

	db.Prepare("update_next_run", R"SQL(
UPDATE cronjobs SET
 next_run=$4::timestamp AT TIME ZONE COALESCE(tz, 'UTC')
WHERE id=$1 AND schedule=$2 AND
 (last_run AT TIME ZONE COALESCE(tz, 'UTC')=$3 OR last_run IS NULL)
 AND enabled AND
 next_run IS NULL
)SQL",
		   4);

	db.Prepare("select_jobs_for_scheduling", R"SQL(
SELECT id, schedule,
 last_run AT TIME ZONE COALESCE(tz, 'UTC'),
 delay, delay_range,
 NOW() AT TIME ZONE COALESCE(tz, 'UTC')
FROM cronjobs WHERE enabled AND next_run IS NULL
LIMIT 1000
)SQL",
		   0);
}

static std::chrono::seconds
RandomSeconds(std::chrono::seconds range)
{
	return std::chrono::seconds(RandomInt64(range.count()));
}

/**
 * Roll the dice to generate a random delay in the given range and
 * write it to the database.
 */
static std::chrono::seconds
MakeRandomDelay(Pg::Connection &db, const char *id, const char *schedule,
		std::chrono::seconds range)
{
	const auto delay = RandomSeconds(range);

	const auto result = db.ExecutePrepared("make_random_delay", id, schedule,
					       delay.count(), range.count());
	if (result.GetAffectedRows() == 0)
		throw LostRace{};

	return delay;
}

bool
CalculateNextRun(const ChildLogger &logger, Pg::Connection &db)
{
	const auto result = db.ExecutePrepared("select_jobs_for_scheduling");
	if (result.IsEmpty())
		return true;

	for (const auto &row : result) {
		const char *id = row.GetValue(0), *_schedule = row.GetValue(1),
			*_last_run = row.GetValueOrNull(2),
			*_delay = row.GetValueOrNull(3),
			*_delay_range = row.GetValueOrNull(4),
			*_now = row.GetValue(5);

		try {
			auto delay = _delay != nullptr
				? Pg::ParseIntervalS(_delay)
				: std::chrono::seconds(0);

			std::chrono::system_clock::time_point last_run =
				_last_run != nullptr
				/* note: subtracting the old delay, because it has
				   been added previously (and will be re-added later),
				   to get a consistent view */
				? Pg::ParseTimestamp(_last_run) - delay
				: std::chrono::system_clock::time_point::min();

			const auto now = Pg::ParseTimestamp(_now);

			const CronSchedule schedule(_schedule);

			/* generate a random delay if there is none yet (or if the
			   range has changed, i.e. the schedule has been
			   modified) */
			if (_delay == nullptr ||
			    !IsSameInterval(_delay_range, schedule.delay_range))
				delay = MakeRandomDelay(db, id, _schedule,
							schedule.delay_range);

			const auto next_run = schedule.Next(last_run, now) + delay;
			std::string next_run_buffer;
			const char *next_run_string = next_run == std::chrono::system_clock::time_point::max()
				? "infinity" /* never again execute the "@once" job */
				: (next_run_buffer = Pg::FormatTimestamp(next_run)).c_str();

			auto r = db.ExecutePrepared("update_next_run", id, _schedule, _last_run,
						    next_run_string);
			if (r.GetAffectedRows() == 0)
				throw LostRace{};
		} catch (LostRace) {
			logger(3, "Lost race to schedule job '", id, "'");
		} catch (...) {
			logger(1, "Failed to schedule job '", id, "': ",
			       std::current_exception());
		}
	}

	return false;
}
