// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/net/UdpHandler.hxx"
#include "event/net/UdpListener.hxx"
#include "event/DeferEvent.hxx"
#include "co/InvokeTask.hxx"

#include <string>
#include <vector>

class UniqueSocketDescriptor;
class WorkshopControlChannelHandler;

/**
 * The control channel through which Workshop job processes can send
 * commands to Workshop.
 */
class WorkshopControlChannelServer final : UdpHandler {
	UdpListener socket;

	WorkshopControlChannelHandler &handler;

	/**
	 * Starts the #task.  This moves the InvokeTask::Start() call
	 * out of the OnControl() stack frame, because the task may
	 * finish synchronously and destroy this object.
	 */
	DeferEvent defer_start;

	Co::InvokeTask task;

public:
	WorkshopControlChannelServer(EventLoop &_event_loop,
				     UniqueSocketDescriptor &&_socket,
				     WorkshopControlChannelHandler &_handler) noexcept;

	/**
	 * Receive and handle all pending datagrams.  This is called
	 * after the job process has exited.
	 */
	bool ReceiveAll() {
		/* cancel pending tasks (e.g. "spawn") - nobody will
		   receive its reply anyway */
		CancelTask();

		return socket.ReceiveAll();
	}

private:
	void InvokeTemporaryError(const char *msg) noexcept;

	void StartTask(Co::InvokeTask &&_task) noexcept;
	void OnDeferredStart() noexcept;
	void OnTaskFinished(std::exception_ptr &&error) noexcept;

	void CancelTask() noexcept {
		defer_start.Cancel();
		task = {};
	}

	Co::InvokeTask OnSpawn(std::vector<std::string> args);
	bool OnControl(std::vector<std::string> &&args) noexcept;

	/* virtual methods from UdpHandler */
	bool OnUdpDatagram(std::span<const std::byte> payload,
			   std::span<UniqueFileDescriptor> fds,
			   SocketAddress address, int uid) override;
	bool OnUdpTruncated() noexcept override;
	void OnUdpError(std::exception_ptr &&error) noexcept override;
};
