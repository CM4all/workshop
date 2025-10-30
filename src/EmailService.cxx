// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "EmailService.hxx"
#include "event/net/ConnectSocket.hxx"
#include "event/net/djb/QmqpClient.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/Logger.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/DisposablePointer.hxx"

#include <cassert>
#include <memory>

class EmailService::Job final
	: public IntrusiveListHook<IntrusiveHookMode::NORMAL>,
	  ConnectSocketHandler, QmqpClientHandler
{
	EmailService &service;
	Email email;
	const Logger logger;

	ConnectSocket connect;
	QmqpClient client;

	/**
	 * This is a #ChildProcessHandle of the child process which
	 * relays QMQP to the containerized qrelay socket.
	 */
	DisposablePointer lease;

public:
	Job(EmailService &_service, Email &&_email,
	    std::string &&logger_domain) noexcept;
	void Start() noexcept;
	void Start(UniqueSocketDescriptor s, DisposablePointer _lease) noexcept;

private:
	/* virtual methods from ConnectSocketHandler */
	void OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept override;
	void OnSocketConnectError(std::exception_ptr error) noexcept override;

	/* virtual methods from QmqpClientHandler */
	void OnQmqpClientSuccess(std::string_view description) noexcept override;
	void OnQmqpClientError(std::exception_ptr error) noexcept override;
};

EmailService::Job::Job(EmailService &_service, Email &&_email,
		       std::string &&logger_domain) noexcept
	:service(_service),
	 email(std::move(_email)),
	 logger(std::move(logger_domain)),
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
EmailService::Job::Start(UniqueSocketDescriptor s, DisposablePointer _lease) noexcept
{
	assert(s.IsDefined());
	assert(!lease);

	lease = std::move(_lease);

	client.Begin({email.message.data(), email.message.length()},
		     {email.sender.data(), email.sender.length()});
	for (const auto &i : email.recipients)
		client.AddRecipient({i.data(), i.length()});

	const auto fd = s.Release().ToFileDescriptor();
	client.Commit(fd, fd);
}

void
EmailService::Job::OnSocketConnectSuccess(UniqueSocketDescriptor _fd) noexcept
{
	Start(std::move(_fd), {});
}

void
EmailService::Job::OnSocketConnectError(std::exception_ptr error) noexcept
{
	logger(1, "Failed to connect to QMQP server: ", error);
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
	logger(1, "QMQP server error: ", error);
	service.DeleteJob(*this);
}

EmailService::EmailService(EventLoop &_event_loop, SocketAddress _address) noexcept
	:event_loop(_event_loop), address(_address) {}

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
EmailService::Submit(Email &&email, std::string &&logger_domain) noexcept
{
	assert(HasRelay());

	auto *job = new Job(*this, std::move(email), std::move(logger_domain));
	jobs.push_front(*job);
	job->Start();
}

void
EmailService::Submit(UniqueSocketDescriptor qmqp_socket,
		     DisposablePointer qmqp_socket_lease,
		     Email &&email, std::string &&logger_domain) noexcept
{
	assert(qmqp_socket.IsDefined());

	auto *job = new Job(*this, std::move(email), std::move(logger_domain));
	jobs.push_front(*job);
	job->Start(std::move(qmqp_socket), std::move(qmqp_socket_lease));
}

inline void
EmailService::DeleteJob(Job &job) noexcept
{
	jobs.erase_and_dispose(jobs.iterator_to(job), DeleteDisposer());
}
