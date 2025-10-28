// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/DeferEvent.hxx"
#include "event/FineTimerEvent.hxx"
#include "pg/AsyncConnection.hxx"
#include "io/Logger.hxx"

#include <string>
#include <chrono>

struct WorkshopJob;
struct Plan;
class EventLoop;

class WorkshopQueueHandler {
public:
	virtual std::shared_ptr<Plan> GetWorkshopPlan(const char *name) noexcept = 0;

	/**
	 * Ask the handler whether it is willing to run the given job.
	 * This will be called before the job is claimed.
	 */
	virtual bool CheckWorkshopJob(const WorkshopJob &job,
				      const Plan &plan) noexcept = 0;

	virtual void StartWorkshopJob(WorkshopJob &&job,
				      std::shared_ptr<Plan> plan) noexcept = 0;
};

class WorkshopQueue final : private Pg::AsyncConnectionHandler {
	const ChildLogger logger;

	const std::string node_name;

	Pg::AsyncConnection db;

	/**
	 * Was the queue enabled by #StateDirectories?
	 */
	bool enabled_state = false;

	/**
	 * Was the queue enabled by the administrator?  Also used
	 * during shutdown.
	 */
	bool enabled_admin = true;

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
	FineTimerEvent timer_event;

	std::string plans_include, plans_exclude, plans_lowprio;
	std::chrono::steady_clock::time_point next_expire_check =
		std::chrono::steady_clock::time_point::min();

	WorkshopQueueHandler &handler;

public:
	WorkshopQueue(const Logger &parent_logger, EventLoop &event_loop,
		      const char *_node_name,
		      const char *conninfo, const char *schema,
		      WorkshopQueueHandler &handler) noexcept;
	~WorkshopQueue() noexcept;

	auto &GetEventLoop() const noexcept {
		return timer_event.GetEventLoop();
	}

	[[gnu::pure]]
	const char *GetNodeName() const noexcept {
		return node_name.c_str();
	}

	void Connect() noexcept {
		db.Connect();
	}

	/**
	 * Configure a "plan" filter.
	 */
	void SetFilter(std::string &&plans_include, std::string &&plans_exclude,
		       std::string &&plans_lowprio) noexcept;

	bool IsEnabled() const noexcept {
		return enabled_state && enabled_admin && !disabled_full;
	}

	void SetStateEnabled(bool _enabled) noexcept;

	/**
	 * Disable the queue as an administrative decision (e.g. daemon
	 * shutdown).
	 */
	void DisableAdmin() noexcept {
		enabled_admin = false;
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
	 * Checks if the given rate limit was reached/exceeded.
	 *
	 * @return a positive duration we have to wait until the rate falls
	 * below the limit and a new job can be started, or a non-positive
	 * value if the rate limits is not yet reached
	 */
	std::chrono::seconds CheckRateLimit(const char *plan_name,
					    std::chrono::seconds duration,
					    unsigned max_count) noexcept;

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

	void AddJobCpuUsage(const WorkshopJob &job,
			    std::chrono::microseconds cpu_usage) noexcept;

	unsigned ReapFinishedJobs(const char *plan_name,
				  const char *reap_finished) noexcept;

private:
	/**
	 * Throws on error.
	 */
	void RunResult(const Pg::Result &result);

	/**
	 * Throws on error.
	 */
	void Run2();

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
	 * If the queue is enabled and ready, call Reschedule().  Call
	 * this method when the queue was disabled, but might be
	 * enabled now.
	 */
	void CheckEnabled() noexcept;

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

	/**
	 * Throws on error.
	 */
	bool GetNextScheduled(int *span_r);

	/* virtual methods from Pg::AsyncConnectionHandler */
	void OnConnect() override;
	void OnDisconnect() noexcept override;
	void OnNotify(const char *name) override;
	void OnError(std::exception_ptr e) noexcept override;
};
