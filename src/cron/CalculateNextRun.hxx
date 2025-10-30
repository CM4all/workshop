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
 *
 * Throws on error.
 *
 * @return true if done, false if there are more records to be
 * calculated (or if there was an error)
 */
bool
CalculateNextRun(const ChildLogger &logger, Pg::Connection &db);
