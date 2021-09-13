/*
 * Copyright 2006-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WORKSHOP_CRON_SCHEDULE_HXX
#define WORKSHOP_CRON_SCHEDULE_HXX

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

#endif
