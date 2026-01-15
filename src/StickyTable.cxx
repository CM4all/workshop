// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "StickyTable.hxx"
#include "pg/Connection.hxx"

namespace StickyTable {

void
Init(Pg::Connection &c)
{
	c.Execute(R"SQL(
CREATE TEMPORARY TABLE sticky_non_local (
  sticky_id varchar(256) NOT NULL
)
)SQL");

	c.Execute(R"SQL(
CREATE UNIQUE INDEX sticky_non_local_sticky_id ON sticky_non_local(sticky_id)
)SQL");

	c.Prepare("insert_sticky_non_local", R"SQL(
INSERT INTO sticky_non_local(sticky_id) VALUES($1)
)SQL",
		  1);
}

void
InsertNonLocal(Pg::Connection &c, const char *sticky_id)
{
	c.ExecutePrepared("insert_sticky_non_local", sticky_id);
}

void
Flush(Pg::Connection &c)
{
	c.Execute("TRUNCATE sticky_non_local");
}

} // namespace StickyTable
