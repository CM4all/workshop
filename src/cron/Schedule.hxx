// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/RangeBitSet.hxx"

#include <chrono>

struct tm;

/**
 * A crontab-style schedule.
 */
struct CronSchedule {
	RangeBitSet<0, 59> minutes;
	RangeBitSet<0, 23> hours;
	RangeBitSet<1, 31> days_of_month;
	RangeBitSet<1, 12> months;
	RangeBitSet<0, 6> days_of_week;

	/**
	 * The maximum duration for generating a random delay.  For
	 * example, an hourly cron job will return std::chrono::hours(1).
	 *
	 * The default is 1 minute, which is the smallest cron
	 * granularity.
	 */
	std::chrono::seconds delay_range = std::chrono::minutes(1);

	/**
	 * Parse a crontab(5) schedule specification.
	 *
	 * Throws std::runtime_error if the string cannot be parsed.
	 */
	explicit CronSchedule(const char *s);

	bool operator==(const CronSchedule &other) const noexcept {
		return minutes == other.minutes &&
			hours == other.hours &&
			days_of_month == other.days_of_month &&
			months == other.months &&
			days_of_week == other.days_of_week;
	}

	/**
	 * Is this a "run once, and never again" job?  (Special schedule
	 * string "@once")
	 */
	bool IsOnce() const noexcept {
		return minutes.none() && hours.none() && days_of_month.none() &&
			months.none() && days_of_week.none();
	}

	[[gnu::pure]]
	bool CheckDate(const struct tm &tm) const noexcept;

	/**
	 * Determine when to run the job next time.
	 *
	 * @param last the time this job was last run; min() if it was
	 * never run
	 * @param now the current time stamp
	 * @return the next time this job should run (may be in the past,
	 * which means this job can be run immediately); returns max() if
	 * the job shall never be executed again
	 */
	[[gnu::pure]]
	std::chrono::system_clock::time_point Next(std::chrono::system_clock::time_point last,
						   std::chrono::system_clock::time_point now) const noexcept;
};
