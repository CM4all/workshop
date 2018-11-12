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

#include "EmailService.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/PrintException.hxx"

#include <memory>

EmailService::Job::Job(EmailService &_service, Email &&_email)
	:service(_service),
	 email(std::move(_email)),
	 connect(service.event_loop, *this),
	 client(service.event_loop, *this)
{
}

void
EmailService::Job::Start()
{
	connect.Connect(service.address, std::chrono::seconds(20));
}

void
EmailService::Job::OnSocketConnectSuccess(UniqueSocketDescriptor &&_fd) noexcept
{
	client.Begin({email.message.data(), email.message.length()},
		     {email.sender.data(), email.sender.length()});
	for (const auto &i : email.recipients)
		client.AddRecipient({i.data(), i.length()});

	const int fd = _fd.Steal();
	client.Commit(fd, fd);
}

void
EmailService::Job::OnSocketConnectError(std::exception_ptr error) noexcept
{
	// TODO add context to log message
	PrintException(error);
	service.DeleteJob(*this);
}

void
EmailService::Job::OnQmqpClientSuccess(StringView description) noexcept
{
	// TODO log?
	(void)description;

	service.DeleteJob(*this);
}

void
EmailService::Job::OnQmqpClientError(std::exception_ptr error) noexcept
{
	// TODO add context to log message
	PrintException(error);
	service.DeleteJob(*this);
}

EmailService::~EmailService()
{
	CancelAll();
}

void
EmailService::CancelAll()
{
	jobs.clear_and_dispose(DeleteDisposer());
}

void
EmailService::Submit(Email &&email)
{
	auto *job = new Job(*this, std::move(email));
	jobs.push_front(*job);
	job->Start();
}

inline void
EmailService::DeleteJob(Job &job)
{
	jobs.erase_and_dispose(jobs.iterator_to(job), DeleteDisposer());
}
