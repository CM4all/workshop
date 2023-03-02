// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/AllocatedSocketAddress.hxx"
#include "event/net/ConnectSocket.hxx"
#include "event/net/djb/QmqpClient.hxx"
#include "util/IntrusiveList.hxx"

#include <string>
#include <forward_list>

struct Email {
	std::string sender;
	std::forward_list<std::string> recipients;
	std::string message;

	explicit Email(const char *_sender)
		:sender(_sender) {}

	void AddRecipient(const char *recipient) {
		recipients.emplace_front(recipient);
	}
};

class EmailService {
	EventLoop &event_loop;
	const AllocatedSocketAddress address;

	class Job final
		: public IntrusiveListHook<IntrusiveHookMode::NORMAL>,
		  ConnectSocketHandler, QmqpClientHandler
	{
		EmailService &service;
		Email email;

		ConnectSocket connect;
		QmqpClient client;

	public:
		Job(EmailService &_service, Email &&_email) noexcept;
		void Start() noexcept;

	private:
		/* virtual methods from ConnectSocketHandler */
		void OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept override;
		void OnSocketConnectError(std::exception_ptr error) noexcept override;

		/* virtual methods from QmqpClientHandler */
		void OnQmqpClientSuccess(std::string_view description) noexcept override;
		void OnQmqpClientError(std::exception_ptr error) noexcept override;
	};

	using JobList = IntrusiveList<Job>;

	JobList jobs;

public:
	EmailService(EventLoop &_event_loop, SocketAddress _address) noexcept
		:event_loop(_event_loop), address(_address) {}

	~EmailService() noexcept;

	void CancelAll() noexcept;

	void Submit(Email &&email) noexcept;

private:
	void DeleteJob(Job &job) noexcept;
};
