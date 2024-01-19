// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Operator.hxx"
#include "Queue.hxx"
#include "Workplace.hxx"
#include "EmailService.hxx"
#include "version.h"

#include <fmt/core.h>

CronOperator::CronOperator(CronQueue &_queue, CronWorkplace &_workplace,
			   CronJob &&_job,
			   std::string_view _tag,
			   std::string &&_start_time) noexcept
	:queue(_queue), workplace(_workplace), job(std::move(_job)),
	 logger(*this),
	 tag(_tag),
	 start_time(std::move(_start_time)),
	 timeout_event(_queue.GetEventLoop(), BIND_THIS_METHOD(OnTimeout))
{
	/* kill after 5 minutes */
	timeout_event.Schedule(std::chrono::minutes(5));
}

void
CronOperator::Finish(int exit_status, const char *log) noexcept
{
	queue.Finish(job);
	queue.InsertResult(job, start_time.c_str(), exit_status, log);

	if (!job.notification.empty()) {
		auto *email_service = workplace.GetEmailService();
		if (email_service != nullptr) {
			// TODO: configurable sender?
			Email email("cm4all-workshop");
			email.AddRecipient(job.notification.c_str());

			email.message += fmt::format("X-CM4all-Workshop: " VERSION "\n"
						     "X-CM4all-Workshop-Job: {}\n"
						     "X-CM4all-Workshop-Account: {}\n",
						     job.id,
						     job.account_id);

			if (exit_status >= 0)
				email.message += fmt::format("X-CM4all-Workshop-Status: {}\n",
							     exit_status);

			email.message += "\n";

			if (log != nullptr)
				email.message += log;

			email_service->Submit(std::move(email));
		}
	}
}

void
CronOperator::OnTimeout() noexcept
{
	logger(2, "Timeout");

	Finish(-1, "Timeout");
}

std::string
CronOperator::MakeLoggerDomain() const noexcept
{
	return fmt::format("cron job={} account={}", job.id, job.account_id);
}
