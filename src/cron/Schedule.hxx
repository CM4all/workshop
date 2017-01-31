/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_CRON_SCHEDULE_HXX
#define WORKSHOP_CRON_SCHEDULE_HXX

#include "util/RangeBitSet.hxx"

#include <inline/compiler.h>

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
     * Parse a crontab(5) schedule specification.
     *
     * Throws std::runtime_error if the string cannot be parsed.
     */
    explicit CronSchedule(const char *s);

    bool operator==(const CronSchedule &other) const {
        return minutes == other.minutes &&
            hours == other.hours &&
            days_of_month == other.days_of_month &&
            months == other.months &&
            days_of_week == other.days_of_week;
    }

    gcc_pure
    bool CheckDate(const struct tm &tm) const;

    gcc_pure
    bool CheckTime(const struct tm &tm) const;

    gcc_pure
    bool Check(const struct tm &tm) const;

    gcc_pure
    bool Check(std::chrono::system_clock::time_point t) const;

    /**
     * Determine when to run the job next time.
     *
     * @param last the time this job was last run; min() if it was
     * never run
     * @param now the current time stamp
     * @return the next time this job should run (may be in the past,
     * which means this job can be run immediately)
     */
    gcc_pure
    std::chrono::system_clock::time_point Next(std::chrono::system_clock::time_point last,
                                               std::chrono::system_clock::time_point now) const;
};

#endif
