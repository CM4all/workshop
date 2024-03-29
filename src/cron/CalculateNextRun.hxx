// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef WORKSHOP_CRON_CALCULATE_NEXT_RUN_HXX
#define WORKSHOP_CRON_CALCULATE_NEXT_RUN_HXX

namespace Pg { class Connection; }
class ChildLogger;

/**
 * Calculate the "next_run" column for all rows where it's missing.
 *
 * Throws on error.
 *
 * @return true if done, false if there are more records to be
 * calculated (or if there was an error)
 */
bool
CalculateNextRun(const ChildLogger &logger, Pg::Connection &db);

#endif
