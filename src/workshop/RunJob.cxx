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

#include "ProgressReader.hxx"
#include "ControlChannelListener.hxx"
#include "ControlChannelServer.hxx"
#include "spawn/CgroupState.hxx"
#include "spawn/Direct.hxx"
#include "spawn/ExitListener.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/Registry.hxx"
#include "event/Loop.hxx"
#include "system/CapabilityGlue.hxx"
#include "system/Error.hxx"
#include "system/SetupProcess.hxx"
#include "util/ConstBuffer.hxx"
#include "util/PrintException.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringCompare.hxx"

#include <memory>

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
			throw FormatRuntimeError("Unrecognized option: %s", s);
	}

	if (args.empty())
		throw std::runtime_error("Job executable not specified");

	for (const char *i : args)
		cmdline.child.args.push_back(i);
}

class RunJobInstance final
	: WorkshopControlChannelListener, ExitListener
{
	EventLoop event_loop;
	ChildProcessRegistry child_process_registry;

	std::unique_ptr<ProgressReader> progress_reader;
	std::unique_ptr<WorkshopControlChannelServer> control_channel;

	int exit_status = EXIT_FAILURE;

public:
	RunJobInstance()
		:child_process_registry(event_loop) {}

	void Start(RunJobCommandLine &&cmdline);

	int Run() {
		event_loop.Dispatch();
		return exit_status;
	}

private:
	void OnProgress(unsigned progress) noexcept {
		fprintf(stderr, "received PROGRESS %u\n", progress);
	}

	/* virtual methods from WorkshopControlChannelListener */
	void OnControlProgress(unsigned progress) noexcept override {
		OnProgress(progress);
	}

	void OnControlSetEnv(const char *s) noexcept override {
		fprintf(stderr, "received SETENV '%s'\n", s);
	}

	void OnControlAgain(std::chrono::seconds d) noexcept override {
		fprintf(stderr, "received AGAIN %lu\n",
			(unsigned long)d.count());
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
			fprintf(stderr, "died from signal %d%s",
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

		WorkshopControlChannelListener &listener = *this;
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

	const auto pid = SpawnChildProcess(std::move(p), {},
					   IsSysAdmin(),
					   SocketDescriptor::Undefined());
	child_process_registry.Add(pid, "job", this);
	child_process_registry.SetVolatile();
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
	fprintf(stderr, "Usage: %s [OPTIONS] PROGRAM [ARGS...]\n\n"
		"Valid options:\n"
		" --help, -h     Show this help text\n"
		" --control      Enable the control channel\n"
		, argv[0]);
	return EXIT_FAILURE;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
