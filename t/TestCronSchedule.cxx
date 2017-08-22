#include "cron/Schedule.hxx"
#include "time/ISO8601.hxx"

#include <gtest/gtest.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

TEST(CronSchedule, Parser)
{
    {
        const CronSchedule s("* * * * *");
        ASSERT_TRUE(s.minutes.all());
        ASSERT_TRUE(s.hours.all());
        ASSERT_TRUE(s.days_of_month.all());
        ASSERT_TRUE(s.months.all());
        ASSERT_TRUE(s.days_of_week.all());
        ASSERT_EQ(s.delay_range, std::chrono::minutes(1));
    }

    {
        const CronSchedule s("0-59 0-23 1-31 1-12 0-6");
        ASSERT_TRUE(s.minutes.all());
        ASSERT_TRUE(s.hours.all());
        ASSERT_TRUE(s.days_of_month.all());
        ASSERT_TRUE(s.months.all());
        ASSERT_TRUE(s.days_of_week.all());
        ASSERT_EQ(s.delay_range, std::chrono::minutes(1));
    }

    {
        const CronSchedule s("0-59/1 */1 1-31 1-12 1-7");
        ASSERT_TRUE(s.minutes.all());
        ASSERT_TRUE(s.hours.all());
        ASSERT_TRUE(s.days_of_month.all());
        ASSERT_TRUE(s.months.all());
        ASSERT_TRUE(s.days_of_week.all());
        ASSERT_EQ(s.delay_range, std::chrono::minutes(1));
    }

    {
        const CronSchedule s("*/20 * * * *");
        ASSERT_EQ(s.minutes.count(), 3);
        ASSERT_TRUE(s.minutes[0]);
        ASSERT_TRUE(s.minutes[20]);
        ASSERT_TRUE(s.minutes[40]);
        ASSERT_TRUE(s.hours.all());
        ASSERT_TRUE(s.days_of_month.all());
        ASSERT_TRUE(s.months.all());
        ASSERT_TRUE(s.days_of_week.all());
        ASSERT_EQ(s.delay_range, std::chrono::minutes(20));
    }

    {
        const CronSchedule s("*/15 * * * *");
        ASSERT_EQ(s.minutes.count(), 4);
        ASSERT_TRUE(s.minutes[0]);
        ASSERT_TRUE(s.minutes[15]);
        ASSERT_TRUE(s.minutes[30]);
        ASSERT_TRUE(s.minutes[45]);
        ASSERT_TRUE(s.hours.all());
        ASSERT_TRUE(s.days_of_month.all());
        ASSERT_TRUE(s.months.all());
        ASSERT_TRUE(s.days_of_week.all());
        ASSERT_EQ(s.delay_range, std::chrono::minutes(15));
    }

    {
        const CronSchedule s("*/19 * * * *");
        ASSERT_EQ(s.minutes.count(), 4);
        ASSERT_TRUE(s.minutes[0]);
        ASSERT_TRUE(s.minutes[19]);
        ASSERT_TRUE(s.minutes[38]);
        ASSERT_TRUE(s.minutes[57]);
        ASSERT_TRUE(s.hours.all());
        ASSERT_TRUE(s.days_of_month.all());
        ASSERT_TRUE(s.months.all());
        ASSERT_TRUE(s.days_of_week.all());
        ASSERT_EQ(s.delay_range, std::chrono::minutes(19));
    }

    /* month names */

    {
        const CronSchedule s("* * * feb *");
        ASSERT_TRUE(s.minutes.all());
        ASSERT_TRUE(s.hours.all());
        ASSERT_TRUE(s.days_of_month.all());
        ASSERT_EQ(s.months.count(), 1);
        ASSERT_TRUE(s.months[2]);
        ASSERT_TRUE(s.days_of_week.all());
        ASSERT_EQ(s.delay_range, std::chrono::minutes(1));
    }

    {
        const CronSchedule s("* * * jun,dec,jan *");
        ASSERT_TRUE(s.minutes.all());
        ASSERT_TRUE(s.hours.all());
        ASSERT_TRUE(s.days_of_month.all());
        ASSERT_EQ(s.months.count(), 3);
        ASSERT_TRUE(s.months[1]);
        ASSERT_TRUE(s.months[6]);
        ASSERT_TRUE(s.months[12]);
        ASSERT_TRUE(s.days_of_week.all());
        ASSERT_EQ(s.delay_range, std::chrono::minutes(1));
    }

    /* day of week names */

    {
        const CronSchedule s("* * * * mon");
        ASSERT_TRUE(s.minutes.all());
        ASSERT_TRUE(s.hours.all());
        ASSERT_TRUE(s.days_of_month.all());
        ASSERT_TRUE(s.months.all());
        ASSERT_EQ(s.days_of_week.count(), 1);
        ASSERT_TRUE(s.days_of_week[1]);
        ASSERT_EQ(s.delay_range, std::chrono::minutes(1));
    }

    {
        const CronSchedule s("* * * * wed,sat,mon");
        ASSERT_TRUE(s.minutes.all());
        ASSERT_TRUE(s.hours.all());
        ASSERT_TRUE(s.days_of_month.all());
        ASSERT_TRUE(s.months.all());
        ASSERT_EQ(s.days_of_week.count(), 3);
        ASSERT_TRUE(s.days_of_week[1]);
        ASSERT_TRUE(s.days_of_week[3]);
        ASSERT_TRUE(s.days_of_week[6]);
        ASSERT_EQ(s.delay_range, std::chrono::minutes(1));
    }

    {
        const CronSchedule s("* * * jun,dec,jan *");
        ASSERT_TRUE(s.minutes.all());
        ASSERT_TRUE(s.hours.all());
        ASSERT_TRUE(s.days_of_month.all());
        ASSERT_EQ(s.months.count(), 3);
        ASSERT_TRUE(s.months[1]);
        ASSERT_TRUE(s.months[6]);
        ASSERT_TRUE(s.months[12]);
        ASSERT_TRUE(s.days_of_week.all());
        ASSERT_EQ(s.delay_range, std::chrono::minutes(1));
    }
}

TEST(CronSchedule, Next1)
{
    /* every minute; several wraparound checks follow */
    const CronSchedule s("* * * * *");
    const auto now = std::chrono::system_clock::from_time_t(1485800000);
    ASSERT_EQ(s.Next(ParseISO8601("2016-10-14T16:41:00Z"), now), ParseISO8601("2016-10-14T16:42:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2016-10-14T16:41:30Z"), now), ParseISO8601("2016-10-14T16:42:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2016-10-14T16:41:59Z"), now), ParseISO8601("2016-10-14T16:42:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2016-10-13T23:59:59Z"), now), ParseISO8601("2016-10-14T00:00:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2016-09-30T23:59:59Z"), now), ParseISO8601("2016-10-01T00:00:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2015-12-31T23:59:59Z"), now), ParseISO8601("2016-01-01T00:00:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2016-02-28T23:59:59Z"), now), ParseISO8601("2016-02-29T00:00:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2016-02-29T23:59:59Z"), now), ParseISO8601("2016-03-01T00:00:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2015-02-28T23:59:59Z"), now), ParseISO8601("2015-03-01T00:00:00Z"));
}

TEST(CronSchedule, Next2)
{
    /* every 6 hours */
    const CronSchedule s("30 */6 * * *");
    const auto now = std::chrono::system_clock::from_time_t(1485800000);
    ASSERT_EQ(s.delay_range, std::chrono::minutes(1));
    ASSERT_EQ(s.Next(ParseISO8601("2016-10-14T14:41:00Z"), now), ParseISO8601("2016-10-14T16:30:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2016-10-14T16:41:00Z"), now), ParseISO8601("2016-10-14T22:30:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2016-10-14T22:41:00Z"), now), ParseISO8601("2016-10-15T04:30:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2016-02-29T23:41:00Z"), now), ParseISO8601("2016-03-01T05:30:00Z"));
}

TEST(CronSchedule, Next3)
{
    /* every month on the 29th*/
    const CronSchedule s("30 6 29 * *");
    const auto now = std::chrono::system_clock::from_time_t(1485800000);
    ASSERT_EQ(s.delay_range, std::chrono::minutes(1));
    ASSERT_EQ(s.Next(ParseISO8601("2016-10-14T14:41:00Z"), now), ParseISO8601("2016-10-29T04:30:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2016-02-01T00:41:00Z"), now), ParseISO8601("2016-02-29T05:30:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2015-02-01T00:41:00Z"), now), ParseISO8601("2015-03-29T05:30:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2015-12-29T05:29:00Z"), now), ParseISO8601("2015-12-29T05:30:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2015-12-29T05:30:00Z"), now), ParseISO8601("2016-01-29T05:30:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2015-12-31T05:30:00Z"), now), ParseISO8601("2016-01-29T05:30:00Z"));
}

TEST(CronSchedule, Next4)
{
    /* every monday */
    const CronSchedule s("30 6 * * 1");
    const auto now = std::chrono::system_clock::from_time_t(1485800000);
    ASSERT_EQ(s.delay_range, std::chrono::minutes(1));
    ASSERT_EQ(s.Next(ParseISO8601("2016-10-14T14:41:00Z"), now), ParseISO8601("2016-10-17T04:30:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2016-02-01T00:41:00Z"), now), ParseISO8601("2016-02-01T05:30:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2016-02-01T05:30:00Z"), now), ParseISO8601("2016-02-08T05:30:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2015-02-01T00:41:00Z"), now), ParseISO8601("2015-02-02T05:30:00Z"));
    ASSERT_EQ(s.Next(ParseISO8601("2015-12-29T05:29:00Z"), now), ParseISO8601("2016-01-04T05:30:00Z"));
}

TEST(CronSchedule, Once)
{
    const CronSchedule s("@once");
    ASSERT_EQ(s.delay_range, std::chrono::seconds(0));
    const auto now = std::chrono::system_clock::from_time_t(1485800000);
    ASSERT_TRUE(s.Next(std::chrono::system_clock::time_point::min(), now) >= now - std::chrono::hours(1));
    ASSERT_TRUE(s.Next(std::chrono::system_clock::time_point::min(), now) <= now + std::chrono::hours(1));

    const auto last = std::chrono::system_clock::from_time_t(1485000000);
    ASSERT_EQ(s.Next(last, now), std::chrono::system_clock::time_point::max());
}

TEST(CronSchedule, Special)
{
    ASSERT_EQ(CronSchedule("@yearly"), CronSchedule("0 0 1 1 *"));
    ASSERT_EQ(CronSchedule("@annually"), CronSchedule("0 0 1 1 *"));
    ASSERT_EQ(CronSchedule("@monthly"), CronSchedule("0 0 1 * *"));
    ASSERT_EQ(CronSchedule("@weekly"), CronSchedule("0 0 * * 0"));
    ASSERT_EQ(CronSchedule("@daily"), CronSchedule("0 0 * * *"));
    ASSERT_EQ(CronSchedule("@midnight"), CronSchedule("0 0 * * *"));
    ASSERT_EQ(CronSchedule("@hourly"), CronSchedule("0 * * * *"));
}
