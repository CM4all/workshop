// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SpawnOperator.hxx"
#include "Result.hxx"
#include "PipePondAdapter.hxx"
#include "spawn/Interface.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/ProcessHandle.hxx"
#include "io/Logger.hxx"
#include "io/Pipe.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/UTF8.hxx"

#include <unistd.h>
#include <sys/wait.h>

CronSpawnOperator::CronSpawnOperator(LazyDomainLogger &_logger) noexcept
	:logger(_logger)
{
}

CronSpawnOperator::~CronSpawnOperator() noexcept = default;

void
CronSpawnOperator::Spawn(EventLoop &event_loop, SpawnService &spawn_service,
			 const char *name, std::string_view site,
			 PreparedChildProcess &&p,
			 SocketDescriptor pond_socket)
{
	UniqueFileDescriptor stderr_w;

	if (!p.stderr_fd.IsDefined()) {
		/* no STDERR destination configured: the default is to capture
		   it and save in the cronresults table */
		UniqueFileDescriptor r;
		std::tie(r, stderr_w) = CreatePipe();

		p.stderr_fd = stderr_w;
		if (!p.stdout_fd.IsDefined())
			/* capture STDOUT as well */
			p.stdout_fd = p.stderr_fd;

		output_capture = std::make_unique<PipePondAdapter>(event_loop,
								   std::move(r),
								   8192,
								   pond_socket,
								   site);
	}

	if (p.ns.mount.home != nullptr) {
		/* change to home directory (if one was set) */
		if (p.chdir == nullptr)
			p.chdir = p.ns.mount.GetJailedHome();

		p.SetEnv("HOME", p.ns.mount.GetJailedHome());
	}

	pid = spawn_service.SpawnChildProcess(name,
					      std::move(p));
	pid->SetExitListener(*this);

	logger(2, "running");
}

void
CronSpawnOperator::OnChildProcessExit(int status) noexcept
{
	CronResult result{
		.exit_status = WEXITSTATUS(status),
	};

	if (WIFSIGNALED(status)) {
		logger(1, "died from signal ",
		       WTERMSIG(status),
		       WCOREDUMP(status) ? " (core dumped)" : "");
		result.exit_status = -1;
	} else if (result.exit_status == 0)
		logger(3, "exited with success");
	else
		logger(2, "exited with status ", result.exit_status);

	if (output_capture)
		result.log = std::move(*output_capture).NormalizeASCII();

	if (result.log != nullptr && !ValidateUTF8(result.log.c_str())) {
		/* TODO: purge illegal UTF-8 sequences instead of
		   replacing the log text? */
		result.log = AllocatedString{"Invalid UTF-8 output"};
		logger(2, result.log.c_str());
	}

	Finish(std::move(result));
}
