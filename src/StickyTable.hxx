// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

namespace Pg { class Connection; }

namespace StickyTable {

void
Init(Pg::Connection &c);

void
InsertNonLocal(Pg::Connection &c, const char *sticky_id);

void
Flush(Pg::Connection &c);

} // namespace StickyTable
