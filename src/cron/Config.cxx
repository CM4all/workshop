// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Config.hxx"

#include <stdexcept>

void
CronPartitionConfig::Check() const
{
	if (database.empty())
		throw std::runtime_error("Missing 'database' setting");

	if (!translation_socket.IsDefined())
		throw std::runtime_error("Missing 'translation_server' setting");

	if (!qmqp_server.IsNull() && use_qrelay)
		throw std::runtime_error{"Cannot configure both 'qmqp_server' and 'use_qrelay'"};
}
