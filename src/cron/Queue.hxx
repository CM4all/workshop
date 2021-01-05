/*
 * Copyright 2006-2021 CM4all GmbH
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

#ifndef CRON_QUEUE_HXX
#define CRON_QUEUE_HXX

#include "event/DeferEvent.hxx"
#include "event/TimerEvent.hxx"
#include "pg/AsyncConnection.hxx"
#include "io/Logger.hxx"
#include "util/Compiler.h"

#include <string>
#include <functional>

struct CronJob;
class EventLoop;

class CronQueue final : private Pg::AsyncConnectionHandler {
	typedef std::function<void(CronJob &&job)> Callback;

	const std::string node_name;

	ChildLogger logger;

	Pg::AsyncConnection db;

	const Callback callback;

	/**
	 * Used to move CheckNotify() calls out of the current stack
	 * frame.
	 */
	DeferEvent check_notify_event;

	TimerEvent scheduler_timer, claim_timer;

	/**
	 * Was the queue disabled by the administrator?  Also used during
	 * shutdown.
	 */
	bool disabled_admin = false;

	/**
	 * Is the queue disabled because the node is busy and all slots
	 * are full?
	 */
	bool disabled_full = false;

public:
	CronQueue(const Logger &parent_logger,
		  EventLoop &event_loop, const char *_node_name,
		  const char *conninfo, const char *schema,
		  Callback _callback) noexcept;
	~CronQueue() noexcept;

	EventLoop &GetEventLoop() noexcept {
		return check_notify_event.GetEventLoop();
	}

	gcc_pure
	const char *GetNodeName() const noexcept {
		return node_name.c_str();
	}

	void Connect() noexcept {
		db.Connect();
	}

	void Close() noexcept;

	gcc_pure
	std::string GetNow() {
		ScheduleCheckNotify();
		const auto result = db.Execute("SELECT now()");
		return result.GetOnlyStringChecked();
	}

	bool IsDisabled() const noexcept {
		return disabled_admin || disabled_full;
	}

	/**
	 * Disable the queue as an administrative decision (e.g. daemon
	 * shutdown).
	 */
	void DisableAdmin() noexcept {
		disabled_admin = true;
	}

	/**
	 * Enable the queue after it has been disabled with DisableAdmin().
	 */
	void EnableAdmin() noexcept;

	/**
	 * Disable the queue, e.g. when the node is busy.
	 */
	void DisableFull() noexcept {
		disabled_full = true;
	}

	/**
	 * Enable the queue after it has been disabled with DisableFull().
	 */
	void EnableFull() noexcept;

	bool Claim(const CronJob &job) noexcept;
	void Finish(const CronJob &job) noexcept;

	/**
	 * Insert a row into the "cronresults" table, describing the
	 * #CronJob execution result.
	 */
	void InsertResult(const CronJob &job, const char *start_time,
			  int exit_status,
			  const char *log) noexcept;

private:
	/**
	 * Checks everything asynchronously: if the connection has failed,
	 * schedule a reconnect.  If there are notifies, schedule a queue run.
	 *
	 * This is an extended version of queue_check_notify(), to be used by
	 * public functions that (unlike the internal functions) do not
	 * reschedule.
	 */
	void CheckNotify() noexcept {
		db.CheckNotify();
	}

	void ScheduleCheckNotify() noexcept {
		check_notify_event.Schedule();
	}

	void ReleaseStale();

	void RunScheduler() noexcept;
	void ScheduleScheduler(bool immediately) noexcept;

	void RunClaim() noexcept;
	void ScheduleClaim() noexcept;

	/**
	 * @return false if no pending job was found
	 */
	bool CheckPending();

	/* virtual methods from Pg::AsyncConnectionHandler */
	void OnConnect() override;
	void OnDisconnect() noexcept override;
	void OnNotify(const char *name) override;
	void OnError(std::exception_ptr e) noexcept override;
};

#endif
