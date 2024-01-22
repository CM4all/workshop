// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Operator.hxx"
#include "Queue.hxx"
#include "Workplace.hxx"
#include "Result.hxx"
#include "Notification.hxx"

#include <fmt/core.h>

using std::string_view_literals::operator""sv;

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
	/* kill after the timeout expires */
	timeout_event.Schedule(job.timeout);
}

void
CronOperator::Finish(const CronResult &result) noexcept
{
	queue.Finish(job);
	queue.InsertResult(job, start_time.c_str(), result);

	if (!job.notification.empty()) {
		auto *email_service = workplace.GetEmailService();
		if (email_service != nullptr) {
			SendNotificationEmail(*email_service, job, result);
		}
	}
}

void
CronOperator::Cancel() noexcept
{
	Finish(CronResult::Error("Canceled"sv));
}

void
CronOperator::InvokeExit() noexcept
{
	workplace.OnExit(this);
}

void
CronOperator::OnTimeout() noexcept
{
	logger(2, "Timeout");

	Finish(CronResult::Error("Timeout"sv));
	InvokeExit();
}

std::string
CronOperator::MakeLoggerDomain() const noexcept
{
	return fmt::format("cron job={} account={}", job.id, job.account_id);
}
