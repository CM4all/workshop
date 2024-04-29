// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string_view>

class EmailService;
struct CronJob;
struct CronResult;

/**
 * Throws on error.
 */
void
SendNotificationEmail(EmailService &service, std::string_view sender,
		      const CronJob &job,
		      const CronResult &result);
