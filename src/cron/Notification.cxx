// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Notification.hxx"
#include "Job.hxx"
#include "Result.hxx"
#include "EmailService.hxx"
#include "version.h"

#include <fmt/core.h>

void
SendNotificationEmail(EmailService &service, const CronJob &job,
		      const CronResult &result) noexcept
{
	assert(!job.notification.empty());

	// TODO: configurable sender?
	Email email("cm4all-workshop");
	email.AddRecipient(job.notification.c_str());

	email.message += fmt::format("X-CM4all-Workshop: " VERSION "\n"
				     "X-CM4all-Workshop-Job: {}\n"
				     "X-CM4all-Workshop-Account: {}\n",
				     job.id,
				     job.account_id);

	if (result.exit_status >= 0)
		email.message += fmt::format("X-CM4all-Workshop-Status: {}\n",
					     result.exit_status);

	email.message += "\n";

	if (result.log != nullptr)
		email.message += result.log;

	service.Submit(std::move(email));
}
