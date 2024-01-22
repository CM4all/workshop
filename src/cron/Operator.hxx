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
struct CronResult;

/**
 * A #CronJob being executed.
 */
class CronOperator
	: public IntrusiveListHook<IntrusiveHookMode::NORMAL>,
	  LoggerDomainFactory
{
	CronQueue &queue;
	CronWorkplace &workplace;

protected:
	const CronJob job;

	LazyDomainLogger logger;

private:
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
	void Cancel() noexcept;

protected:
	void Finish(const CronResult &result) noexcept;
	void InvokeExit() noexcept;

private:
	void OnTimeout() noexcept;

	/* virtual methods from LoggerDomainFactory */
	std::string MakeLoggerDomain() const noexcept;
};
