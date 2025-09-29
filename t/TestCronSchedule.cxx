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
		EXPECT_TRUE(s.minutes.all());
		EXPECT_TRUE(s.hours.all());
		EXPECT_TRUE(s.days_of_month.all());
		EXPECT_TRUE(s.months.all());
		EXPECT_TRUE(s.days_of_week.all());
		EXPECT_EQ(s.delay_range, std::chrono::minutes(1));
	}

	{
		const CronSchedule s("0-59 0-23 1-31 1-12 0-6");
		EXPECT_TRUE(s.minutes.all());
		EXPECT_TRUE(s.hours.all());
		EXPECT_TRUE(s.days_of_month.all());
		EXPECT_TRUE(s.months.all());
		EXPECT_TRUE(s.days_of_week.all());
		EXPECT_EQ(s.delay_range, std::chrono::minutes(1));
	}

	{
		const CronSchedule s("0-59/1 */1 1-31 1-12 1-7");
		EXPECT_TRUE(s.minutes.all());
		EXPECT_TRUE(s.hours.all());
		EXPECT_TRUE(s.days_of_month.all());
		EXPECT_TRUE(s.months.all());
		EXPECT_TRUE(s.days_of_week.all());
		EXPECT_EQ(s.delay_range, std::chrono::minutes(1));
	}

	{
		const CronSchedule s("*/20 * * * *");
		EXPECT_EQ(s.minutes.count(), 3u);
		EXPECT_TRUE(s.minutes[0]);
		EXPECT_TRUE(s.minutes[20]);
		EXPECT_TRUE(s.minutes[40]);
		EXPECT_TRUE(s.hours.all());
		EXPECT_TRUE(s.days_of_month.all());
		EXPECT_TRUE(s.months.all());
		EXPECT_TRUE(s.days_of_week.all());
		EXPECT_EQ(s.delay_range, std::chrono::minutes(20));
	}

	{
		const CronSchedule s("*/15 * * * *");
		EXPECT_EQ(s.minutes.count(), 4u);
		EXPECT_TRUE(s.minutes[0]);
		EXPECT_TRUE(s.minutes[15]);
		EXPECT_TRUE(s.minutes[30]);
		EXPECT_TRUE(s.minutes[45]);
		EXPECT_TRUE(s.hours.all());
		EXPECT_TRUE(s.days_of_month.all());
		EXPECT_TRUE(s.months.all());
		EXPECT_TRUE(s.days_of_week.all());
		EXPECT_EQ(s.delay_range, std::chrono::minutes(15));
	}

	{
		const CronSchedule s("*/19 * * * *");
		EXPECT_EQ(s.minutes.count(), 4u);
		EXPECT_TRUE(s.minutes[0]);
		EXPECT_TRUE(s.minutes[19]);
		EXPECT_TRUE(s.minutes[38]);
		EXPECT_TRUE(s.minutes[57]);
		EXPECT_TRUE(s.hours.all());
		EXPECT_TRUE(s.days_of_month.all());
		EXPECT_TRUE(s.months.all());
		EXPECT_TRUE(s.days_of_week.all());
		EXPECT_EQ(s.delay_range, std::chrono::minutes(19));
	}

	/* month names */

	{
		const CronSchedule s("* * * feb *");
		EXPECT_TRUE(s.minutes.all());
		EXPECT_TRUE(s.hours.all());
		EXPECT_TRUE(s.days_of_month.all());
		EXPECT_EQ(s.months.count(), 1u);
		EXPECT_TRUE(s.months[2]);
		EXPECT_TRUE(s.days_of_week.all());
		EXPECT_EQ(s.delay_range, std::chrono::minutes(1));
	}

	{
		const CronSchedule s("* * * jun,dec,jan *");
		EXPECT_TRUE(s.minutes.all());
		EXPECT_TRUE(s.hours.all());
		EXPECT_TRUE(s.days_of_month.all());
		EXPECT_EQ(s.months.count(), 3u);
		EXPECT_TRUE(s.months[1]);
		EXPECT_TRUE(s.months[6]);
		EXPECT_TRUE(s.months[12]);
		EXPECT_TRUE(s.days_of_week.all());
		EXPECT_EQ(s.delay_range, std::chrono::minutes(1));
	}

	/* day of week names */

	{
		const CronSchedule s("* * * * mon");
		EXPECT_TRUE(s.minutes.all());
		EXPECT_TRUE(s.hours.all());
		EXPECT_TRUE(s.days_of_month.all());
		EXPECT_TRUE(s.months.all());
		EXPECT_EQ(s.days_of_week.count(), 1u);
		EXPECT_TRUE(s.days_of_week[1]);
		EXPECT_EQ(s.delay_range, std::chrono::minutes(1));
	}

	{
		const CronSchedule s("* * * * wed,sat,mon");
		EXPECT_TRUE(s.minutes.all());
		EXPECT_TRUE(s.hours.all());
		EXPECT_TRUE(s.days_of_month.all());
		EXPECT_TRUE(s.months.all());
		EXPECT_EQ(s.days_of_week.count(), 3u);
		EXPECT_TRUE(s.days_of_week[1]);
		EXPECT_TRUE(s.days_of_week[3]);
		EXPECT_TRUE(s.days_of_week[6]);
		EXPECT_EQ(s.delay_range, std::chrono::minutes(1));
	}

	{
		const CronSchedule s("* * * jun,dec,jan *");
		EXPECT_TRUE(s.minutes.all());
		EXPECT_TRUE(s.hours.all());
		EXPECT_TRUE(s.days_of_month.all());
		EXPECT_EQ(s.months.count(), 3u);
		EXPECT_TRUE(s.months[1]);
		EXPECT_TRUE(s.months[6]);
		EXPECT_TRUE(s.months[12]);
		EXPECT_TRUE(s.days_of_week.all());
		EXPECT_EQ(s.delay_range, std::chrono::minutes(1));
	}

	// month + day names are case insensitive

	{
		const CronSchedule s("* * * feb,MAY TUE,tHu");
		EXPECT_TRUE(s.minutes.all());
		EXPECT_TRUE(s.hours.all());
		EXPECT_TRUE(s.days_of_month.all());
		EXPECT_EQ(s.months.count(), 2u);
		EXPECT_TRUE(s.months[2]);
		EXPECT_TRUE(s.months[5]);
		EXPECT_EQ(s.days_of_week.count(), 2u);
		EXPECT_TRUE(s.days_of_week[2]);
		EXPECT_TRUE(s.days_of_week[4]);
		EXPECT_EQ(s.delay_range, std::chrono::minutes(1));
	}
}

static auto
ParseTime(const char *s)
{
	return ParseISO8601(s).first;
}

TEST(CronSchedule, Next1)
{
	/* every minute; several wraparound checks follow */
	const CronSchedule s("* * * * *");
	const auto now = std::chrono::system_clock::from_time_t(1485800000);
	EXPECT_EQ(s.Next(ParseTime("2016-10-14T16:41:00Z"), now), ParseTime("2016-10-14T16:42:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-10-14T16:41:30Z"), now), ParseTime("2016-10-14T16:42:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-10-14T16:41:59Z"), now), ParseTime("2016-10-14T16:42:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-10-13T23:59:59Z"), now), ParseTime("2016-10-14T00:00:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-09-30T23:59:59Z"), now), ParseTime("2016-10-01T00:00:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2015-12-31T23:59:59Z"), now), ParseTime("2016-01-01T00:00:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-02-28T23:59:59Z"), now), ParseTime("2016-02-29T00:00:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-02-29T23:59:59Z"), now), ParseTime("2016-03-01T00:00:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2015-02-28T23:59:59Z"), now), ParseTime("2015-03-01T00:00:00Z"));
}

TEST(CronSchedule, Next2)
{
	/* every 6 hours */
	const CronSchedule s("30 */6 * * *");
	const auto now = std::chrono::system_clock::from_time_t(1485800000);
	EXPECT_EQ(s.delay_range, std::chrono::minutes(1));
	EXPECT_EQ(s.Next(ParseTime("2016-10-14T14:41:00Z"), now), ParseTime("2016-10-14T18:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-10-14T16:41:00Z"), now), ParseTime("2016-10-14T18:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-10-14T18:41:00Z"), now), ParseTime("2016-10-15T00:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-10-15T00:41:00Z"), now), ParseTime("2016-10-15T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-02-29T23:41:00Z"), now), ParseTime("2016-03-01T00:30:00Z"));
}

TEST(CronSchedule, Next3)
{
	/* every month on the 29th*/
	const CronSchedule s("30 6 29 * *");
	const auto now = std::chrono::system_clock::from_time_t(1485800000);
	EXPECT_EQ(s.delay_range, std::chrono::minutes(1));
	EXPECT_EQ(s.Next(ParseTime("2016-10-14T14:41:00Z"), now), ParseTime("2016-10-29T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-02-01T00:41:00Z"), now), ParseTime("2016-02-29T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2015-02-01T00:41:00Z"), now), ParseTime("2015-03-29T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2015-12-29T05:30:00Z"), now), ParseTime("2015-12-29T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2015-12-29T06:29:00Z"), now), ParseTime("2015-12-29T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2015-12-29T06:30:00Z"), now), ParseTime("2016-01-29T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2015-12-31T06:30:00Z"), now), ParseTime("2016-01-29T06:30:00Z"));
}

TEST(CronSchedule, Next4)
{
	/* every monday */
	const CronSchedule s("30 6 * * 1");
	const auto now = std::chrono::system_clock::from_time_t(1485800000);
	EXPECT_EQ(s.delay_range, std::chrono::minutes(1));
	EXPECT_EQ(s.Next(ParseTime("2016-10-14T14:41:00Z"), now), ParseTime("2016-10-17T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-02-01T00:41:00Z"), now), ParseTime("2016-02-01T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-02-01T05:30:00Z"), now), ParseTime("2016-02-01T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2015-02-01T00:41:00Z"), now), ParseTime("2015-02-02T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2015-12-28T05:29:00Z"), now), ParseTime("2015-12-28T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2015-12-28T06:29:59Z"), now), ParseTime("2015-12-28T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2015-12-29T05:29:00Z"), now), ParseTime("2016-01-04T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2015-12-29T06:29:00Z"), now), ParseTime("2016-01-04T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2015-12-29T06:30:00Z"), now), ParseTime("2016-01-04T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2015-12-29T06:31:00Z"), now), ParseTime("2016-01-04T06:30:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2015-12-31T06:30:01Z"), now), ParseTime("2016-01-04T06:30:00Z"));
}

TEST(CronSchedule, Next5)
{
	/* every 5 minutes in one certain hour of day */
	const CronSchedule s("*/5 6 * * *");
	const auto now = std::chrono::system_clock::from_time_t(1485800000);
	EXPECT_EQ(s.delay_range, std::chrono::minutes(5));
	EXPECT_EQ(s.Next(ParseTime("2016-10-14T06:40:00Z"), now), ParseTime("2016-10-14T06:45:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-10-14T06:55:00Z"), now), ParseTime("2016-10-15T06:00:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-10-14T14:00:00Z"), now), ParseTime("2016-10-15T06:00:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-10-14T14:01:00Z"), now), ParseTime("2016-10-15T06:00:00Z"));
	EXPECT_EQ(s.Next(ParseTime("2016-10-14T14:41:00Z"), now), ParseTime("2016-10-15T06:00:00Z"));
}

TEST(CronSchedule, Once)
{
	const CronSchedule s("@once");
	EXPECT_EQ(s.delay_range, std::chrono::seconds(0));
	const auto now = std::chrono::system_clock::from_time_t(1485800000);
	EXPECT_TRUE(s.Next(std::chrono::system_clock::time_point::min(), now) >= now - std::chrono::hours(1));
	EXPECT_TRUE(s.Next(std::chrono::system_clock::time_point::min(), now) <= now + std::chrono::hours(1));

	const auto last = std::chrono::system_clock::from_time_t(1485000000);
	EXPECT_EQ(s.Next(last, now), std::chrono::system_clock::time_point::max());
}

TEST(CronSchedule, Special)
{
	const CronSchedule yearly{"@yearly"};
	EXPECT_EQ(yearly, CronSchedule("0 0 1 1 *"));
	EXPECT_EQ(yearly.minutes.count(), 1);
	EXPECT_TRUE(yearly.minutes[0]);
	EXPECT_EQ(yearly.hours.count(), 1);
	EXPECT_TRUE(yearly.hours[0]);
	EXPECT_EQ(yearly.days_of_month.count(), 1);
	EXPECT_TRUE(yearly.days_of_month[1]);
	EXPECT_EQ(yearly.months.count(), 1);
	EXPECT_TRUE(yearly.months[1]);
	EXPECT_TRUE(yearly.days_of_week.all());
	EXPECT_EQ(yearly.delay_range, std::chrono::hours{24 * 365});

	const CronSchedule annually{"@annually"};
	EXPECT_EQ(annually, CronSchedule("0 0 1 1 *"));
	EXPECT_EQ(annually.minutes.count(), 1);
	EXPECT_TRUE(annually.minutes[0]);
	EXPECT_EQ(annually.hours.count(), 1);
	EXPECT_TRUE(annually.hours[0]);
	EXPECT_EQ(annually.days_of_month.count(), 1);
	EXPECT_TRUE(annually.days_of_month[1]);
	EXPECT_EQ(annually.months.count(), 1);
	EXPECT_TRUE(annually.months[1]);
	EXPECT_TRUE(annually.days_of_week.all());
	EXPECT_EQ(annually.delay_range, std::chrono::hours(24 * 365));

	const CronSchedule monthly{"@monthly"};
	EXPECT_EQ(monthly, CronSchedule("0 0 1 * *"));
	EXPECT_EQ(monthly.minutes.count(), 1);
	EXPECT_TRUE(monthly.minutes[0]);
	EXPECT_EQ(monthly.hours.count(), 1);
	EXPECT_TRUE(monthly.hours[0]);
	EXPECT_EQ(monthly.days_of_month.count(), 1);
	EXPECT_TRUE(monthly.days_of_month[1]);
	EXPECT_TRUE(monthly.months.all());
	EXPECT_TRUE(monthly.days_of_week.all());
	EXPECT_EQ(monthly.delay_range, std::chrono::hours(24 * 28));

	const CronSchedule weekly{"@weekly"};
	EXPECT_EQ(weekly, CronSchedule("0 0 * * 0"));
	EXPECT_EQ(weekly.minutes.count(), 1);
	EXPECT_TRUE(weekly.minutes[0]);
	EXPECT_EQ(weekly.hours.count(), 1);
	EXPECT_TRUE(weekly.hours[0]);
	EXPECT_TRUE(weekly.days_of_month.all());
	EXPECT_TRUE(weekly.months.all());
	EXPECT_EQ(weekly.days_of_week.count(), 1);
	EXPECT_TRUE(weekly.days_of_week[0]);
	EXPECT_EQ(weekly.delay_range, std::chrono::hours(24 * 7));

	const CronSchedule daily{"@daily"};
	EXPECT_EQ(daily, CronSchedule("0 0 * * *"));
	EXPECT_EQ(daily.minutes.count(), 1);
	EXPECT_TRUE(daily.minutes[0]);
	EXPECT_EQ(daily.hours.count(), 1);
	EXPECT_TRUE(daily.hours[0]);
	EXPECT_TRUE(daily.days_of_month.all());
	EXPECT_TRUE(daily.months.all());
	EXPECT_TRUE(daily.days_of_week.all());
	EXPECT_EQ(daily.delay_range, std::chrono::hours(24));

	const CronSchedule midnight{"@midnight"};
	EXPECT_EQ(midnight, CronSchedule("0 0 * * *"));
	EXPECT_EQ(midnight.minutes.count(), 1);
	EXPECT_TRUE(midnight.minutes[0]);
	EXPECT_EQ(midnight.hours.count(), 1);
	EXPECT_TRUE(midnight.hours[0]);
	EXPECT_TRUE(midnight.days_of_month.all());
	EXPECT_TRUE(midnight.months.all());
	EXPECT_TRUE(midnight.days_of_week.all());
	EXPECT_EQ(midnight.delay_range, std::chrono::hours(1));

	const CronSchedule hourly{"@hourly"};
	EXPECT_EQ(hourly, CronSchedule("0 * * * *"));
	EXPECT_EQ(hourly.minutes.count(), 1);
	EXPECT_TRUE(hourly.minutes[0]);
	EXPECT_TRUE(hourly.hours.all());
	EXPECT_TRUE(hourly.days_of_month.all());
	EXPECT_TRUE(hourly.months.all());
	EXPECT_TRUE(hourly.days_of_week.all());
	EXPECT_EQ(hourly.delay_range, std::chrono::hours(1));
}
