// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Job.hxx"
#include "event/FarTimerEvent.hxx"
#include "io/Logger.hxx"
#include "util/IntrusiveList.hxx"

#include <string>

class ChildProcessRegistry;
class CronQueue;
class CronWorkplace;

/**
 * A #CronJob being executed.
 */
class CronOperator
	: public IntrusiveListHook<IntrusiveHookMode::NORMAL>,
	  LoggerDomainFactory
{
protected:
	CronQueue &queue;
	CronWorkplace &workplace;
	const CronJob job;

	LazyDomainLogger logger;

	const std::string tag;

	const std::string start_time;

	FarTimerEvent timeout_event;

public:
	CronOperator(CronQueue &_queue, CronWorkplace &_workplace, CronJob &&_job,
		     std::string_view _tag,
		     std::string &&_start_time) noexcept;

	virtual ~CronOperator() noexcept = default;

	CronOperator(const CronOperator &other) = delete;
	CronOperator &operator=(const CronOperator &other) = delete;

	EventLoop &GetEventLoop() const noexcept {
		return timeout_event.GetEventLoop();
	}

	bool IsTag(std::string_view _tag) const noexcept {
		return tag == _tag;
	}

	/**
	 * Cancel job execution, e.g. by sending SIGTERM to the child
	 * process.  This also abandons the child process, i.e. after this
	 * method returns, cancellation can be considered complete, even
	 * if the child process continues to run (because it ignores the
	 * kill signal).
	 */
	void Cancel() noexcept {
		Finish(-1, "Canceled");
	}

protected:
	void Finish(int exit_status, const char *log) noexcept;

private:
	void OnTimeout() noexcept;

	/* virtual methods from LoggerDomainFactory */
	std::string MakeLoggerDomain() const noexcept;
};
