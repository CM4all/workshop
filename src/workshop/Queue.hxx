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

#ifndef WORKSHOP_QUEUE_HXX
#define WORKSHOP_QUEUE_HXX

#include "event/DeferEvent.hxx"
#include "event/TimerEvent.hxx"
#include "pg/AsyncConnection.hxx"
#include "io/Logger.hxx"
#include "util/Compiler.h"

#include <functional>
#include <string>
#include <chrono>

struct WorkshopJob;
class EventLoop;

class WorkshopQueue final : private Pg::AsyncConnectionHandler {
	typedef std::function<void(WorkshopJob &&job)> Callback;

	const ChildLogger logger;

	const std::string node_name;

	Pg::AsyncConnection db;

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

	bool running = false;

	/** if set to true, the current queue run should be interrupted,
	    to be started again */
	bool interrupt = false;

	/**
	 * Used to move CheckNotify() calls out of the current stack
	 * frame.
	 */
	DeferEvent check_notify_event;

	/**
	 * Timer event which runs the queue.
	 */
	TimerEvent timer_event;

	std::string plans_include, plans_exclude, plans_lowprio;
	std::chrono::steady_clock::time_point next_expire_check =
		std::chrono::steady_clock::time_point::min();

	const Callback callback;

public:
	WorkshopQueue(const Logger &parent_logger, EventLoop &event_loop,
		      const char *_node_name,
		      const char *conninfo, const char *schema,
		      Callback _callback) noexcept;
	~WorkshopQueue() noexcept;

	auto &GetEventLoop() const noexcept {
		return timer_event.GetEventLoop();
	}

	gcc_pure
	const char *GetNodeName() const noexcept {
		return node_name.c_str();
	}

	void Connect() noexcept {
		db.Connect();
	}

	void Close() noexcept;

	/**
	 * Configure a "plan" filter.
	 */
	void SetFilter(std::string &&plans_include, std::string &&plans_exclude,
		       std::string &&plans_lowprio) noexcept;

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

	/**
	 * @return true on success
	 */
	bool SetJobProgress(const WorkshopJob &job, unsigned progress,
			    const char *timeout) noexcept;

	void SetJobEnv(const WorkshopJob &job, const char *more_env);

	/**
	 * Disassociate from the job, act as if this node had never
	 * claimed it.  It will notify the other workshop nodes.
	 */
	void RollbackJob(const WorkshopJob &job) noexcept;

	/**
	 * Reschedule the given job after it has been executed already.
	 *
	 * @param delay don't execute this job until the given duration
	 * has passed
	 */
	void AgainJob(const WorkshopJob &job, const char *log,
		      std::chrono::seconds delay) noexcept;

	void SetJobDone(const WorkshopJob &job, int status,
			const char *log) noexcept;

private:
	void RunResult(const Pg::Result &result) noexcept;
	void Run2() noexcept;
	void Run() noexcept;

	void OnTimer() noexcept;

	void ScheduleTimer(Event::Duration d) noexcept {
		timer_event.Schedule(d);
	}

	/**
	 * Schedule a queue run.  It will occur "very soon" (in a few
	 * milliseconds).
	 */
	void Reschedule() noexcept {
		ScheduleTimer(std::chrono::milliseconds(10));
	}

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

	int GetNextScheduled(int *span_r) noexcept;

	/* virtual methods from Pg::AsyncConnectionHandler */
	void OnConnect() override;
	void OnDisconnect() noexcept override;
	void OnNotify(const char *name) override;
	void OnError(std::exception_ptr e) noexcept override;
};

#endif
