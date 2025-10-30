// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <string>
#include <string_view>

class EmailService;
class SpawnService;
struct ChildOptions;
struct CronJob;
struct CronResult;

/**
 * Throws on error.
 */
void
SendNotificationEmail(EmailService &service, bool use_qrelay, std::string_view sender,
		      SpawnService &spawn_service, const ChildOptions &child_options,
		      std::string &&logger_domain,
		      const CronJob &job,
		      const CronResult &result);
