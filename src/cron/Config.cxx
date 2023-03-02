// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Config.hxx"

#include <stdexcept>

void
CronPartitionConfig::Check() const
{
	if (database.empty())
		throw std::runtime_error("Missing 'database' setting");

	if (translation_socket == nullptr)
		throw std::runtime_error("Missing 'translation_server' setting");
}
