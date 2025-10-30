// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "net/AllocatedSocketAddress.hxx"
#include "util/IntrusiveList.hxx"

#include <string>
#include <forward_list>

class EventLoop;
class UniqueSocketDescriptor;
class DisposablePointer;

struct Email {
	std::string sender;
	std::forward_list<std::string> recipients;
	std::string message;

	explicit Email(std::string_view _sender) noexcept
		:sender(_sender) {}

	void AddRecipient(const char *recipient) {
		recipients.emplace_front(recipient);
	}
};

class EmailService {
	EventLoop &event_loop;
	const AllocatedSocketAddress address;

	class Job;
	using JobList = IntrusiveList<Job>;

	JobList jobs;

public:
	EmailService(EventLoop &_event_loop, SocketAddress _address) noexcept;
	~EmailService() noexcept;

	bool HasRelay() const noexcept {
		return !address.IsNull();
	}

	void CancelAll() noexcept;

	void Submit(Email &&email, std::string &&logger_domain) noexcept;
	void Submit(UniqueSocketDescriptor qmqp_socket,
		    DisposablePointer qmqp_socket_lease,
		    Email &&email,
		    std::string &&logger_domain) noexcept;

private:
	void DeleteJob(Job &job) noexcept;
};
