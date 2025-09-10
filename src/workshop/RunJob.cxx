// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ProgressReader.hxx"
#include "ControlChannelHandler.hxx"
#include "ControlChannelServer.hxx"
#include "spawn/CgroupState.hxx"
#include "spawn/Direct.hxx"
#include "spawn/ExitListener.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/Registry.hxx"
#include "spawn/PidfdEvent.hxx"
#include "event/Loop.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "system/SetupProcess.hxx"
#include "net/SocketPair.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/Pipe.hxx"
#include "util/PrintException.hxx"
#include "util/StringCompare.hxx"
#include "config.h"

#include <span>

#ifdef HAVE_LIBCAP
#include "lib/cap/Glue.hxx"
#endif

#include <memory>
#include <optional>

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strerror()
#include <sys/socket.h>
#include <sys/wait.h>

struct Usage {};

struct RunJobCommandLine {
	bool control = false;
	PreparedChildProcess child;

	RunJobCommandLine() noexcept {
		child.stdout_fd = FileDescriptor(STDOUT_FILENO);
		child.stderr_fd = FileDescriptor{STDERR_FILENO};
		child.no_new_privs = true;
	}
};

static void
ParseCommandLine(RunJobCommandLine &cmdline, std::span<const char *const> args)
{
	while (!args.empty() && *args.front() == '-') {
		const char *s = args.front();
		args = args.subspan(1);

		if (StringIsEqual(s, "--help") || StringIsEqual(s, "-h")) {
			throw Usage();
		} else if (StringIsEqual(s, "--control")) {
			cmdline.control = true;
		} else
			throw FmtRuntimeError("Unrecognized option: {}", s);
	}

	if (args.empty())
		throw std::runtime_error("Job executable not specified");

	for (const char *i : args)
		cmdline.child.args.push_back(i);
}

class RunJobInstance final
	: WorkshopControlChannelHandler, ExitListener
{
	EventLoop event_loop;
	ChildProcessRegistry child_process_registry;

	std::unique_ptr<ProgressReader> progress_reader;
	std::unique_ptr<WorkshopControlChannelServer> control_channel;

	std::optional<PidfdEvent> pid;

	int exit_status = EXIT_FAILURE;

public:
	void Start(RunJobCommandLine &&cmdline);

	int Run() noexcept {
		event_loop.Run();
		return exit_status;
	}

private:
	void OnProgress(unsigned progress) noexcept {
		fmt::print(stderr, "received PROGRESS {}\n", progress);
	}

	/* virtual methods from WorkshopControlChannelHandler */
	void OnControlProgress(unsigned progress) noexcept override {
		OnProgress(progress);
	}

	void OnControlSetEnv(const char *s) noexcept override {
		fmt::print(stderr, "received SETENV '{}'\n", s);
	}

	void OnControlAgain(std::chrono::seconds d) noexcept override {
		fmt::print(stderr, "received AGAIN {}\n", d.count());
	}

	[[noreturn]]
	UniqueFileDescriptor OnControlSpawn(const char *, const char *) override {
		throw std::runtime_error{"spawn not implemented"};
	}

	void OnControlTemporaryError(std::exception_ptr &&error) noexcept override {
		PrintException(std::move(error));
	}

	void OnControlPermanentError(std::exception_ptr &&error) noexcept override {
		control_channel.reset();
		PrintException(std::move(error));
	}

	void OnControlClosed() noexcept override {
		control_channel.reset();
	}

	/* virtual methods from ExitListener */
	void OnChildProcessExit(int status) noexcept override {
		if (status < 0) {
			fmt::print(stderr, "exited with errno {}\n", strerror(-status));
			exit_status = EXIT_FAILURE;
		} else if (WIFSIGNALED(status)) {
			fmt::print(stderr, "died from signal {}{}\n",
				   WTERMSIG(status),
				   WCOREDUMP(status) ? " (core dumped)" : "");
			exit_status = EXIT_FAILURE;
		} else if (WIFEXITED(status))
			exit_status = WEXITSTATUS(status);
	}
};

void
RunJobInstance::Start(RunJobCommandLine &&cmdline)
{
	auto &p = cmdline.child;

	UniqueSocketDescriptor control_child;
	UniqueFileDescriptor stdout_w;

	if (cmdline.control) {
		UniqueSocketDescriptor control_parent;
		std::tie(control_parent, control_child) = CreateSocketPair(SOCK_SEQPACKET);

		p.control_fd = control_child.ToFileDescriptor();

		WorkshopControlChannelHandler &listener = *this;
		control_channel = std::make_unique<WorkshopControlChannelServer>(event_loop,
										 std::move(control_parent),
										 listener);
	} else {
		/* if there is no control channel, read progress from the
		   stdout pipe */
		UniqueFileDescriptor stdout_r;
		std::tie(stdout_r, stdout_w) = CreatePipe();

		p.stdout_fd = stdout_w;

		progress_reader = std::make_unique<ProgressReader>(event_loop, std::move(stdout_r),
								   BIND_THIS_METHOD(OnProgress));
	}

#ifdef HAVE_LIBCAP
	const bool is_sys_admin = IsSysAdmin();
#else
	const bool is_sys_admin = true;
#endif

	ExitListener &exit_listener = *this;
	pid.emplace(event_loop,
		    std::move(SpawnChildProcess(std::move(p), {}, false, is_sys_admin).pidfd),
		    "foo", exit_listener);
}

int
main(int argc, char **argv)
try {

	SetupProcess();
	RunJobInstance instance;

	{
		RunJobCommandLine cmdline;
		ParseCommandLine(cmdline, std::span<const char *const>{argv + 1, static_cast<std::size_t>(argc - 1)});
		instance.Start(std::move(cmdline));
	}

	return instance.Run();
} catch (Usage) {
	fmt::print(stderr, "Usage: {} [OPTIONS] PROGRAM [ARGS...]\n\n"
		   "Valid options:\n"
		   " --help, -h     Show this help text\n"
		   " --control      Enable the control channel\n"
		   , argv[0]);
	return EXIT_FAILURE;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
