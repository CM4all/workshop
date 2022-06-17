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

#ifndef WORKSHOP_OPERATOR_HXX
#define WORKSHOP_OPERATOR_HXX

#include "Job.hxx"
#include "LogBridge.hxx"
#include "ControlChannelHandler.hxx"
#include "spawn/ExitListener.hxx"
#include "spawn/ProcessHandle.hxx"
#include "event/FarTimerEvent.hxx"
#include "io/Logger.hxx"
#include "util/IntrusiveList.hxx"

#include <memory>
#include <string>
#include <list>
#include <chrono>

struct Plan;
class WorkshopWorkplace;
class ProgressReader;
class WorkshopControlChannelServer;
class UniqueFileDescriptor;
class UniqueSocketDescriptor;
class ChildProcessHandle;

/** an operator is a job being executed */
class WorkshopOperator final
	: public IntrusiveListHook,
	  public ExitListener,
	WorkshopControlChannelHandler,
	LoggerDomainFactory
{
	EventLoop &event_loop;

	WorkshopWorkplace &workplace;
	WorkshopJob job;
	std::shared_ptr<Plan> plan;
	std::unique_ptr<ChildProcessHandle> pid;

	bool exited = false;

	/**
	 * Shall the job be executed again?
	 *
	 * The value is negative if the job shall NOT be executed again,
	 * and positive to delay the repeated execution.
	 */
	std::chrono::seconds again = std::chrono::seconds(-1);

	LazyDomainLogger logger;

	FarTimerEvent timeout_event;

	std::unique_ptr<ProgressReader> progress_reader;

	std::unique_ptr<WorkshopControlChannelServer> control_channel;

	/**
	 * The write end of the stderr pipe.  This is only set if the
	 * process is allowed to use the "spawn" command, to be able
	 * to redirect its stderr to the same pipe.
	 */
	UniqueFileDescriptor stderr_write_pipe;

	LogBridge log;

	class SpawnedProcess;

	/**
	 * Child processes spawned by this job.  They will need to be
	 * killed when the job process exits.
	 */
	IntrusiveList<SpawnedProcess> children;

public:
	WorkshopOperator(EventLoop &_event_loop,
			 WorkshopWorkplace &_workplace, const WorkshopJob &_job,
			 const std::shared_ptr<Plan> &_plan,
			 UniqueFileDescriptor stderr_read_pipe,
			 UniqueFileDescriptor _stderr_write_pipe,
			 UniqueSocketDescriptor control_socket,
			 size_t max_log_buffer,
			 bool enable_journal) noexcept;

	WorkshopOperator(const WorkshopOperator &other) = delete;

	~WorkshopOperator() noexcept;

	WorkshopOperator &operator=(const WorkshopOperator &other) = delete;

	const Plan &GetPlan() const noexcept {
		return *plan;
	}

	const std::string &GetPlanName() const noexcept {
		return job.plan_name;
	}

	void SetPid(std::unique_ptr<ChildProcessHandle> &&_pid) noexcept {
		pid = std::move(_pid);
		pid->SetExitListener(*this);
	}

	void SetOutput(UniqueFileDescriptor fd) noexcept;

	void CreateSyslogClient(const char *me,
				int facility,
				const char *host_and_port);

	void Expand(std::list<std::string> &args) const noexcept;

private:
	void ScheduleTimeout() noexcept;
	void OnTimeout() noexcept;
	void OnProgress(unsigned progress) noexcept;

public:
	/* virtual methods from ExitListener */
	void OnChildProcessExit(int status) noexcept override;

private:
	/* virtual methods from LoggerDomainFactory */
	std::string MakeLoggerDomain() const noexcept override;

	/* virtual methods from WorkshopControlChannelHandler */
	void OnControlProgress(unsigned progress) noexcept override;
	void OnControlSetEnv(const char *s) noexcept override;
	void OnControlAgain(std::chrono::seconds d) noexcept override;
	UniqueFileDescriptor OnControlSpawn(const char *token,
					    const char *param) override;
	void OnControlTemporaryError(std::exception_ptr e) noexcept override;
	void OnControlPermanentError(std::exception_ptr e) noexcept override;
	void OnControlClosed() noexcept override;
};

#endif
