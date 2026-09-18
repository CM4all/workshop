// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

namespace Pg { class Connection; }

class StickyTable {
	/**
	 * Tracks whether the table is empty.  Will be initialized by
	 * Init().
	 */
	bool empty;

public:
	void Init(Pg::Connection &c);
	void InsertNonLocal(Pg::Connection &c, const char *sticky_id);
	void Flush(Pg::Connection &c);
};
