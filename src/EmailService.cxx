// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "EmailService.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/PrintException.hxx"

#include <memory>

EmailService::Job::Job(EmailService &_service, Email &&_email) noexcept
	:service(_service),
	 email(std::move(_email)),
	 connect(service.event_loop, *this),
	 client(service.event_loop, *this)
{
}

void
EmailService::Job::Start() noexcept
{
	connect.Connect(service.address, std::chrono::seconds(20));
}

void
EmailService::Job::OnSocketConnectSuccess(UniqueSocketDescriptor _fd) noexcept
{
	client.Begin({email.message.data(), email.message.length()},
		     {email.sender.data(), email.sender.length()});
	for (const auto &i : email.recipients)
		client.AddRecipient({i.data(), i.length()});

	const auto fd = _fd.Release().ToFileDescriptor();
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
EmailService::Job::OnQmqpClientSuccess(std::string_view description) noexcept
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

EmailService::~EmailService() noexcept
{
	CancelAll();
}

void
EmailService::CancelAll() noexcept
{
	jobs.clear_and_dispose(DeleteDisposer());
}

void
EmailService::Submit(Email &&email) noexcept
{
	auto *job = new Job(*this, std::move(email));
	jobs.push_front(*job);
	job->Start();
}

inline void
EmailService::DeleteJob(Job &job) noexcept
{
	jobs.erase_and_dispose(jobs.iterator_to(job), DeleteDisposer());
}
