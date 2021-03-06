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

#include "SpawnOperator.hxx"
#include "Workplace.hxx"
#include "PipePondAdapter.hxx"
#include "spawn/Interface.hxx"
#include "spawn/Prepared.hxx"
#include "system/Error.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/Exception.hxx"
#include "util/UTF8.hxx"

#include <unistd.h>
#include <sys/wait.h>

CronSpawnOperator::CronSpawnOperator(CronQueue &_queue,
				     CronWorkplace &_workplace,
				     SpawnService &_spawn_service,
				     CronJob &&_job,
				     std::string &&_start_time) noexcept
	:CronOperator(_queue, _workplace,
		      std::move(_job),
		      std::move(_start_time)),
	 spawn_service(_spawn_service)
{
}

CronSpawnOperator::~CronSpawnOperator() noexcept = default;

void
CronSpawnOperator::Spawn(PreparedChildProcess &&p,
			 SocketDescriptor pond_socket)
	try {
		if (p.stderr_fd < 0) {
			/* no STDERR destination configured: the default is to capture
			   it and save in the cronresults table */
			UniqueFileDescriptor r, w;
			if (!UniqueFileDescriptor::CreatePipe(r, w))
				throw MakeErrno("pipe() failed");

			p.SetStderr(std::move(w));
			if (p.stdout_fd < 0)
				/* capture STDOUT as well */
				p.stdout_fd = p.stderr_fd;

			output_capture = std::make_unique<PipePondAdapter>(GetEventLoop(),
									   std::move(r),
									   8192,
									   pond_socket,
									   job.account_id);
		}

		/* change to home directory (if one was set) */
		p.chdir = p.ns.GetJailedHome();

		pid = spawn_service.SpawnChildProcess(job.id.c_str(),
						      std::move(p),
						      SocketDescriptor::Undefined(),
						      this);

		logger(2, "running");

		/* kill after 5 minutes */
		timeout_event.Schedule(std::chrono::minutes(5));
	} catch (...) {
		Finish(-1, GetFullMessage(std::current_exception()).c_str());
		throw;
	}

void
CronSpawnOperator::Cancel() noexcept
{
	output_capture.reset();
	spawn_service.KillChildProcess(pid, SIGTERM);

	Finish(-1, "Canceled");
	timeout_event.Cancel();
	workplace.OnExit(this);
}

void
CronSpawnOperator::OnChildProcessExit(int status) noexcept
{
	int exit_status = WEXITSTATUS(status);

	if (WIFSIGNALED(status)) {
		logger(1, "died from signal ",
		       WTERMSIG(status),
		       WCOREDUMP(status) ? " (core dumped)" : "");
		exit_status = -1;
	} else if (exit_status == 0)
		logger(3, "exited with success");
	else
		logger(2, "exited with status ", exit_status);

	const char *log = output_capture
		? output_capture->NormalizeASCII()
		: nullptr;

	if (log != nullptr && !ValidateUTF8(log)) {
		/* TODO: purge illegal UTF-8 sequences instead of
		   replacing the log text? */
		log = "Invalid UTF-8 output";
		logger(2, log);
	}

	Finish(exit_status, log);
	timeout_event.Cancel();
	workplace.OnExit(this);
}
