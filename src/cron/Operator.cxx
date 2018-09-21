/*
 * Copyright 2006-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Operator.hxx"
#include "Queue.hxx"
#include "Workplace.hxx"
#include "EmailService.hxx"
#include "util/StringFormat.hxx"

CronOperator::CronOperator(CronQueue &_queue, CronWorkplace &_workplace,
			   CronJob &&_job,
			   std::string &&_start_time) noexcept
	:queue(_queue), workplace(_workplace), job(std::move(_job)),
	 logger(*this),
	 start_time(std::move(_start_time)),
	 timeout_event(GetEventLoop(), BIND_THIS_METHOD(OnTimeout))
{
}

EventLoop &
CronOperator::GetEventLoop()
{
	return queue.GetEventLoop();
}

void
CronOperator::Finish(int exit_status, const char *log)
{
	queue.Finish(job);
	queue.InsertResult(job, start_time.c_str(), exit_status, log);

	if (!job.notification.empty()) {
		auto *email_service = workplace.GetEmailService();
		if (email_service != nullptr) {
			// TODO: configurable sender?
			Email email("cm4all-workshop");
			email.AddRecipient(job.notification.c_str());

			char buffer[1024];

			snprintf(buffer, sizeof(buffer),
				 "X-CM4all-Workshop: " VERSION "\n"
				 "X-CM4all-Workshop-Job: %s\n"
				 "X-CM4all-Workshop-Account: %s\n",
				 job.id.c_str(),
				 job.account_id.c_str());
			email.message += buffer;

			if (exit_status >= 0) {
				snprintf(buffer, sizeof(buffer), "X-CM4all-Workshop-Status: %d\n",
					 exit_status);
				email.message += buffer;
			}

			email.message += "\n";

			if (log != nullptr)
				email.message += log;

			email_service->Submit(std::move(email));
		}
	}
}

void
CronOperator::OnTimeout()
{
	logger(2, "Timeout");

	Cancel();
}

std::string
CronOperator::MakeLoggerDomain() const noexcept
{
	return StringFormat<64>("cron job=%s", job.id.c_str()).c_str();
}
