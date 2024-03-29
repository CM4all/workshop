// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Config.hxx"

#include <stdexcept>

void
WorkshopPartitionConfig::Check() const
{
	if (database.empty())
		throw std::runtime_error("Missing 'database' setting");
}
