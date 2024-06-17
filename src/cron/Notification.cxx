// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Notification.hxx"
#include "Job.hxx"
#include "Result.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "spawn/ProcessHandle.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "uri/EmailAddress.hxx"
#include "util/DisposablePointer.hxx"
#include "EmailService.hxx"
#include "NsQrelayConnect.hxx"
#include "version.h"

#include <fmt/core.h>

using std::string_view_literals::operator""sv;

[[nodiscard]] [[gnu::pure]]
static Email
MakeNotificationEmail(std::string_view sender,
		      const CronJob &job, const CronResult &result) noexcept
{
	Email email{sender};
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

	return email;
}

void
SendNotificationEmail(EmailService &service, bool use_qrelay, std::string_view sender,
		      SpawnService &spawn_service, const ChildOptions &child_options,
		      std::string &&logger_domain,
		      const CronJob &job,
		      const CronResult &result)
{
	assert(!job.notification.empty());

	if (!VerifyEmailAddress(job.notification))
		throw FmtInvalidArgument("Malformed email address: {:?}", job.notification);

	if (use_qrelay) {
		auto [socket, process] = NsConnectQrelay(spawn_service, job.id.c_str(),
							 child_options);
		service.Submit(std::move(socket),
			       ToDeletePointer(process.release()),
			       MakeNotificationEmail(sender, job, result),
			       std::move(logger_domain));
	} else if (service.HasRelay())
		service.Submit(MakeNotificationEmail(sender, job, result),
			       std::move(logger_domain));
	else
		throw std::invalid_argument{"No qmqp_server configured"};
}
