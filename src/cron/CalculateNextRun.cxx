/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "CalculateNextRun.hxx"
#include "Schedule.hxx"
#include "pg/Connection.hxx"
#include "time/Convert.hxx"
#include "time/ISO8601.hxx"

#include <assert.h>
#include <stdio.h>

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
CalculateNextRun(PgConnection &db)
{
    const auto result =
        db.Execute("SELECT id, schedule, last_run "
                   "FROM cronjobs WHERE enabled AND next_run IS NULL "
                   "LIMIT 1000");
    if (!result.IsQuerySuccessful()) {
        fprintf(stderr, "SELECT on cronjobs failed: %s\n",
                result.GetErrorMessage());
        return false;
    }

    if (result.IsEmpty())
        return true;

    const auto now = std::chrono::system_clock::now();

    for (const auto &row : result) {
        const char *id = row.GetValue(0), *_schedule = row.GetValue(1),
            *_last_run = row.GetValue(2);

        if (row.IsValueNull(2))
            _last_run = nullptr;

        try {
            std::chrono::system_clock::time_point last_run =
                _last_run != nullptr
                ? ParsePgTimestamp(_last_run)
                : std::chrono::system_clock::time_point::min();
            if (last_run == std::chrono::system_clock::time_point::min())
                last_run = now - std::chrono::minutes(1);

            const CronSchedule schedule(_schedule);

            const auto next_run = schedule.Next(last_run);

            auto r = db.ExecuteParams("UPDATE cronjobs SET next_run=$4 "
                                      "WHERE id=$1 AND schedule=$2 AND"
                                      " (last_run=$3 OR last_run IS NULL) AND enabled AND"
                                      " next_run IS NULL",
                                      id, _schedule, _last_run,
                                      FormatISO8601(next_run).c_str());
            if (!r.IsCommandSuccessful())
                throw std::runtime_error(r.GetErrorMessage());

            if (r.GetAffectedRows() == 0)
                fprintf(stderr, "Lost race to schedule job '%s'\n", id);
        } catch (const std::runtime_error &e) {
            fprintf(stderr, "Failed to schedule job '%s': %s\n",
                    id, e.what());
        }
    }

    return false;
}
