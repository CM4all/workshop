// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

namespace Pg { class Connection; }
class ChildLogger;

/**
 * Prepare SQL statements used by CalculateNextRun().
 */
void
InitCalculateNextRun(Pg::Connection &db);

/**
 * Calculate the "next_run" column for all rows where it's missing.
 * Rows which cannot be scheduled (e.g. because the schedule cannot
 * be parsed) get "next_run=infinity", which removes them from the
 * scheduler until the client clears "next_run".
 *
 * Throws on (database) error.
 *
 * @return true if done, false if there may be more records to be
 * calculated
 */
bool
CalculateNextRun(const ChildLogger &logger, Pg::Connection &db);
