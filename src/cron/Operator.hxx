/*
 * Copyright 2006-2022 CM4all GmbH
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

#ifndef CRON_OPERATOR_HXX
#define CRON_OPERATOR_HXX

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
	: public IntrusiveListHook,
	  LoggerDomainFactory
{
protected:
	CronQueue &queue;
	CronWorkplace &workplace;
	const CronJob job;

	LazyDomainLogger logger;

	const std::string start_time;

	FarTimerEvent timeout_event;

public:
	CronOperator(CronQueue &_queue, CronWorkplace &_workplace, CronJob &&_job,
		     std::string &&_start_time) noexcept;

	virtual ~CronOperator() noexcept = default;

	CronOperator(const CronOperator &other) = delete;
	CronOperator &operator=(const CronOperator &other) = delete;

	EventLoop &GetEventLoop() noexcept;

	/**
	 * Cancel job execution, e.g. by sending SIGTERM to the child
	 * process.  This also abandons the child process, i.e. after this
	 * method returns, cancellation can be considered complete, even
	 * if the child process continues to run (because it ignores the
	 * kill signal).
	 */
	virtual void Cancel() noexcept = 0;

protected:
	void Finish(int exit_status, const char *log) noexcept;

private:
	void OnTimeout() noexcept;

	/* virtual methods from LoggerDomainFactory */
	std::string MakeLoggerDomain() const noexcept;
};

#endif
