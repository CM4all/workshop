// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/Chrono.hxx"

#include <string>

struct CronJob {
	std::string id, account_id, command, translate_param;

	/**
	 * Email address to receive notification.
	 */
	std::string notification;

	Event::Duration timeout{};
};
