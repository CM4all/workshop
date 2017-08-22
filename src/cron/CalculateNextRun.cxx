/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "CalculateNextRun.hxx"
#include "Schedule.hxx"
#include "pg/Connection.hxx"
#include "pg/Error.hxx"
#include "io/Logger.hxx"
#include "time/Convert.hxx"
#include "time/ISO8601.hxx"

#include <assert.h>

static std::chrono::system_clock::time_point
ParsePgTimestamp(const char *s)
{
    assert(s != nullptr);

    struct tm tm;
    const char *end = strptime(s, "%F %T", &tm);
    if (end == nullptr)
        throw std::runtime_error("Failed to parse PostgreSQL timestamp");

    return TimeGm(tm);
}

bool
CalculateNextRun(const Logger &logger, Pg::Connection &db)
{
    const auto result =
        db.Execute("SELECT id, schedule, last_run "
                   "FROM cronjobs WHERE enabled AND next_run IS NULL "
                   "LIMIT 1000");
    if (!result.IsQuerySuccessful()) {
        logger(1, "SELECT on cronjobs failed: ", result.GetErrorMessage());
        return false;
    }

    if (result.IsEmpty())
        return true;

    const auto now = std::chrono::system_clock::now();

    for (const auto &row : result) {
        const char *id = row.GetValue(0), *_schedule = row.GetValue(1),
            *_last_run = row.GetValueOrNull(2);

        try {
            std::chrono::system_clock::time_point last_run =
                _last_run != nullptr
                ? ParsePgTimestamp(_last_run)
                : std::chrono::system_clock::time_point::min();

            const CronSchedule schedule(_schedule);

            const auto next_run = schedule.Next(last_run, now);
            // TODO: check next_run==max() and don't write this bogus value into the database

            auto r = db.ExecuteParams("UPDATE cronjobs SET next_run=$4 "
                                      "WHERE id=$1 AND schedule=$2 AND"
                                      " (last_run=$3 OR last_run IS NULL) AND enabled AND"
                                      " next_run IS NULL",
                                      id, _schedule, _last_run,
                                      FormatISO8601(next_run).c_str());
            if (!r.IsCommandSuccessful())
                throw Pg::Error(std::move(r));

            if (r.GetAffectedRows() == 0)
                throw std::runtime_error("Lost race to schedule job");
        } catch (...) {
            logger(1, "Failed to schedule job '", id, "': ",
                   std::current_exception());
        }
    }

    return false;
}
