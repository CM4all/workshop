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
#include "lib/cap/Glue.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "system/Error.hxx"
#include "system/SetupProcess.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/ConstBuffer.hxx"
#include "util/PrintException.hxx"
#include "util/StringCompare.hxx"

#include <memory>
#include <optional>

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>

struct Usage {};

struct RunJobCommandLine {
	bool control = false;
	PreparedChildProcess child;

	RunJobCommandLine() noexcept {
		child.SetStdout(STDOUT_FILENO);
		child.SetStderr(STDERR_FILENO);
		child.no_new_privs = true;
	}
};

static void
ParseCommandLine(RunJobCommandLine &cmdline, ConstBuffer<const char *> args)
{
	while (!args.empty() && *args.front() == '-') {
		const char *s = args.shift();

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

	UniqueFileDescriptor OnControlSpawn(const char *, const char *) override {
		throw std::runtime_error{"spawn not implemented"};
	}

	void OnControlTemporaryError(std::exception_ptr e) noexcept override {
		PrintException(e);
	}

	void OnControlPermanentError(std::exception_ptr e) noexcept override {
		control_channel.reset();
		PrintException(e);
	}

	void OnControlClosed() noexcept override {
		control_channel.reset();
	}

	/* virtual methods from ExitListener */
	void OnChildProcessExit(int status) noexcept override {
		if (WIFSIGNALED(status)) {
			fmt::print(stderr, "died from signal {}{}",
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

	if (cmdline.control) {
		UniqueSocketDescriptor control_parent, control_child;

		if (!UniqueSocketDescriptor::CreateSocketPair(AF_LOCAL, SOCK_SEQPACKET, 0,
							      control_parent, control_child))
			throw MakeErrno("socketpair() failed");

		p.SetControl(std::move(control_child));

		WorkshopControlChannelHandler &listener = *this;
		control_channel = std::make_unique<WorkshopControlChannelServer>(event_loop,
										 std::move(control_parent),
										 listener);
	} else {
		/* if there is no control channel, read progress from the
		   stdout pipe */
		UniqueFileDescriptor stdout_r, stdout_w;
		if (!UniqueFileDescriptor::CreatePipe(stdout_r, stdout_w))
			throw MakeErrno("pipe() failed");

		p.SetStdout(std::move(stdout_w));

		progress_reader = std::make_unique<ProgressReader>(event_loop, std::move(stdout_r),
								   BIND_THIS_METHOD(OnProgress));
	}

	ExitListener &exit_listener = *this;
	pid.emplace(event_loop,
		    std::move(SpawnChildProcess(std::move(p), {}, IsSysAdmin()).first),
		    "foo", exit_listener);
}

int
main(int argc, char **argv)
try {

	SetupProcess();
	RunJobInstance instance;

	{
		RunJobCommandLine cmdline;
		ParseCommandLine(cmdline, ConstBuffer<const char *>(argv + 1, argc - 1));
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
