/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_CRON_CALCULATE_NEXT_RUN_HXX
#define WORKSHOP_CRON_CALCULATE_NEXT_RUN_HXX

namespace Pg { class Connection; };
class Logger;

/**
 * Calculate the "next_run" column for all rows where it's missing.
 *
 * @return true if done, false if there are more records to be
 * calculated (or if there was an error)
 */
bool
CalculateNextRun(const Logger &logger, Pg::Connection &db);

#endif
