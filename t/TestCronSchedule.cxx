#include "cron/Schedule.hxx"
#include "time/ISO8601.hxx"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void
ok1(bool value)
{
    if (!value)
        abort();
}

static void
TestCronScheduleParser()
{
    {
        const CronSchedule s("* * * * *");
        ok1(s.minutes.all());
        ok1(s.hours.all());
        ok1(s.days_of_month.all());
        ok1(s.months.all());
        ok1(s.days_of_week.all());
    }

    {
        const CronSchedule s("0-59 0-23 1-31 1-12 0-6");
        ok1(s.minutes.all());
        ok1(s.hours.all());
        ok1(s.days_of_month.all());
        ok1(s.months.all());
        ok1(s.days_of_week.all());
    }

    {
        const CronSchedule s("0-59/1 */1 1-31 1-12 1-7");
        ok1(s.minutes.all());
        ok1(s.hours.all());
        ok1(s.days_of_month.all());
        ok1(s.months.all());
        ok1(s.days_of_week.all());
    }

    {
        const CronSchedule s("*/20 * * * *");
        ok1(s.minutes.count() == 3);
        ok1(s.minutes[0]);
        ok1(s.minutes[20]);
        ok1(s.minutes[40]);
        ok1(s.hours.all());
        ok1(s.days_of_month.all());
        ok1(s.months.all());
        ok1(s.days_of_week.all());
    }

    {
        const CronSchedule s("*/15 * * * *");
        ok1(s.minutes.count() == 4);
        ok1(s.minutes[0]);
        ok1(s.minutes[15]);
        ok1(s.minutes[30]);
        ok1(s.minutes[45]);
        ok1(s.hours.all());
        ok1(s.days_of_month.all());
        ok1(s.months.all());
        ok1(s.days_of_week.all());
    }

    {
        const CronSchedule s("*/19 * * * *");
        ok1(s.minutes.count() == 4);
        ok1(s.minutes[0]);
        ok1(s.minutes[19]);
        ok1(s.minutes[38]);
        ok1(s.minutes[57]);
        ok1(s.hours.all());
        ok1(s.days_of_month.all());
        ok1(s.months.all());
        ok1(s.days_of_week.all());
    }
}

static void
TestCronScheduleNext1()
{
    /* every minute; several wraparound checks follow */
    const CronSchedule s("* * * * *");
    ok1(s.Next(ParseISO8601("2016-10-14T16:41:00Z")) == ParseISO8601("2016-10-14T16:42:00Z"));
    ok1(s.Next(ParseISO8601("2016-10-14T16:41:30Z")) == ParseISO8601("2016-10-14T16:42:00Z"));
    ok1(s.Next(ParseISO8601("2016-10-14T16:41:59Z")) == ParseISO8601("2016-10-14T16:42:00Z"));
    ok1(s.Next(ParseISO8601("2016-10-13T23:59:59Z")) == ParseISO8601("2016-10-14T00:00:00Z"));
    ok1(s.Next(ParseISO8601("2016-09-30T23:59:59Z")) == ParseISO8601("2016-10-01T00:00:00Z"));
    ok1(s.Next(ParseISO8601("2015-12-31T23:59:59Z")) == ParseISO8601("2016-01-01T00:00:00Z"));
    ok1(s.Next(ParseISO8601("2016-02-28T23:59:59Z")) == ParseISO8601("2016-02-29T00:00:00Z"));
    ok1(s.Next(ParseISO8601("2016-02-29T23:59:59Z")) == ParseISO8601("2016-03-01T00:00:00Z"));
    ok1(s.Next(ParseISO8601("2015-02-28T23:59:59Z")) == ParseISO8601("2015-03-01T00:00:00Z"));
}

static void
TestCronScheduleNext2()
{
    /* every 6 hours */
    const CronSchedule s("30 */6 * * *");
    ok1(s.Next(ParseISO8601("2016-10-14T14:41:00Z")) == ParseISO8601("2016-10-14T16:30:00Z"));
    ok1(s.Next(ParseISO8601("2016-10-14T16:41:00Z")) == ParseISO8601("2016-10-14T22:30:00Z"));
    ok1(s.Next(ParseISO8601("2016-10-14T22:41:00Z")) == ParseISO8601("2016-10-15T04:30:00Z"));
    ok1(s.Next(ParseISO8601("2016-02-29T23:41:00Z")) == ParseISO8601("2016-03-01T05:30:00Z"));
}

static void
TestCronScheduleNext3()
{
    /* every month on the 29th*/
    const CronSchedule s("30 6 29 * *");
    ok1(s.Next(ParseISO8601("2016-10-14T14:41:00Z")) == ParseISO8601("2016-10-29T04:30:00Z"));
    ok1(s.Next(ParseISO8601("2016-02-01T00:41:00Z")) == ParseISO8601("2016-02-29T05:30:00Z"));
    ok1(s.Next(ParseISO8601("2015-02-01T00:41:00Z")) == ParseISO8601("2015-03-29T05:30:00Z"));
    ok1(s.Next(ParseISO8601("2015-12-29T05:29:00Z")) == ParseISO8601("2015-12-29T05:30:00Z"));
    ok1(s.Next(ParseISO8601("2015-12-29T05:30:00Z")) == ParseISO8601("2016-01-29T05:30:00Z"));
    ok1(s.Next(ParseISO8601("2015-12-31T05:30:00Z")) == ParseISO8601("2016-01-29T05:30:00Z"));
}

static void
TestCronScheduleNext4()
{
    /* every monday */
    const CronSchedule s("30 6 * * 1");
    ok1(s.Next(ParseISO8601("2016-10-14T14:41:00Z")) == ParseISO8601("2016-10-17T04:30:00Z"));
    ok1(s.Next(ParseISO8601("2016-02-01T00:41:00Z")) == ParseISO8601("2016-02-01T05:30:00Z"));
    ok1(s.Next(ParseISO8601("2016-02-01T05:30:00Z")) == ParseISO8601("2016-02-08T05:30:00Z"));
    ok1(s.Next(ParseISO8601("2015-02-01T00:41:00Z")) == ParseISO8601("2015-02-02T05:30:00Z"));
    ok1(s.Next(ParseISO8601("2015-12-29T05:29:00Z")) == ParseISO8601("2016-01-04T05:30:00Z"));
}

static void
TestCronScheduleSpecial()
{
    ok1(CronSchedule("@yearly") == CronSchedule("0 0 1 1 *"));
    ok1(CronSchedule("@annually") == CronSchedule("0 0 1 1 *"));
    ok1(CronSchedule("@monthly") == CronSchedule("0 0 1 * *"));
    ok1(CronSchedule("@weekly") == CronSchedule("0 0 * * 0"));
    ok1(CronSchedule("@daily") == CronSchedule("0 0 * * *"));
    ok1(CronSchedule("@midnight") == CronSchedule("0 0 * * *"));
    ok1(CronSchedule("@hourly") == CronSchedule("0 * * * *"));
}

int
main(int, char **)
{
    setenv("TZ", "CET", true);

    TestCronScheduleParser();
    TestCronScheduleNext1();
    TestCronScheduleNext2();
    TestCronScheduleNext3();
    TestCronScheduleNext4();
    TestCronScheduleSpecial();

    return EXIT_SUCCESS;
}
