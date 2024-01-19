// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SpawnOperator.hxx"
#include "Workplace.hxx"
#include "PipePondAdapter.hxx"
#include "spawn/Interface.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/ProcessHandle.hxx"
#include "io/Pipe.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/Exception.hxx"
#include "util/UTF8.hxx"

#include <unistd.h>
#include <sys/wait.h>

CronSpawnOperator::CronSpawnOperator(CronQueue &_queue,
				     CronWorkplace &_workplace,
				     SpawnService &_spawn_service,
				     CronJob &&_job,
				     std::string_view _tag,
				     std::string &&_start_time) noexcept
	:CronOperator(_queue, _workplace,
		      std::move(_job),
		      _tag,
		      std::move(_start_time)),
	 spawn_service(_spawn_service)
{
}

CronSpawnOperator::~CronSpawnOperator() noexcept = default;

void
CronSpawnOperator::Spawn(PreparedChildProcess &&p,
			 SocketDescriptor pond_socket)
try {
	if (!p.stderr_fd.IsDefined()) {
		/* no STDERR destination configured: the default is to capture
		   it and save in the cronresults table */
		auto [r, w] = CreatePipe();

		p.SetStderr(std::move(w));
		if (!p.stdout_fd.IsDefined())
			/* capture STDOUT as well */
			p.stdout_fd = p.stderr_fd;

		output_capture = std::make_unique<PipePondAdapter>(GetEventLoop(),
								   std::move(r),
								   8192,
								   pond_socket,
								   job.account_id);
	}

	if (p.ns.mount.home != nullptr) {
		/* change to home directory (if one was set) */
		if (p.chdir == nullptr)
			p.chdir = p.ns.mount.GetJailedHome();

		p.SetEnv("HOME", p.ns.mount.GetJailedHome());
	}

	pid = spawn_service.SpawnChildProcess(job.id.c_str(),
					      std::move(p));
	pid->SetExitListener(*this);

	logger(2, "running");
} catch (...) {
	Finish(-1, GetFullMessage(std::current_exception()).c_str());
	throw;
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
	workplace.OnExit(this);
}
