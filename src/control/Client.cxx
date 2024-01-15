// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "system/Error.hxx"
#include "io/Iovec.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/control/Client.hxx"
#include "util/ByteOrder.hxx"
#include "util/ConstBuffer.hxx"
#include "util/CRC32.hxx"
#include "util/Macros.hxx"
#include "util/SpanCast.hxx"
#include "util/StringCompare.hxx"
#include "util/PrintException.hxx"

#include <fmt/core.h>

#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>

struct Usage {
	const char *msg = nullptr;
};

static void
SimpleCommand(const char *server, ConstBuffer<const char *> args,
	      BengProxy::ControlCommand cmd)
{
	if (!args.empty())
		throw Usage{"Too many arguments"};

	BengControlClient client{server};
	client.Send(cmd);
}

static void
Nop(const char *server, ConstBuffer<const char *> args)
{
	SimpleCommand(server, args, BengProxy::ControlCommand::NOP);
}

static void
Verbose(const char *server, ConstBuffer<const char *> args)
{
	if (args.empty())
		throw Usage{"Log level missing"};

	const char *s = args.shift();

	if (!args.empty())
		throw Usage{"Too many arguments"};

	uint8_t log_level = atoi(s);

	BengControlClient client{server};
	client.Send(BengProxy::ControlCommand::VERBOSE,
		    ReferenceAsBytes(log_level));
}

static void
DisableQueue(const char *server, ConstBuffer<const char *> args)
{
	SimpleCommand(server, args, BengProxy::ControlCommand::DISABLE_QUEUE);
}

static void
EnableQueue(const char *server, ConstBuffer<const char *> args)
{
	SimpleCommand(server, args, BengProxy::ControlCommand::ENABLE_QUEUE);
}

static void
TerminateChildren(const char *server, ConstBuffer<const char *> args)
{
	if (args.empty())
		throw Usage{"Tag missing"};

	BengControlClient client{server};

	for (const std::string_view tag : args)
		client.Send(BengProxy::ControlCommand::TERMINATE_CHILDREN, tag);
}

int
main(int argc, char **argv)
try {
	ConstBuffer<const char *> args(argv + 1, argc - 1);

	const char *server = "@cm4all-workshop.control";

	while (!args.empty() && args.front()[0] == '-') {
		const char *option = args.shift();
		if (const char *new_server = StringAfterPrefix(option, "--server=")) {
			server = new_server;
		} else
			throw Usage{"Unknown option"};
	}

	if (args.empty())
		throw Usage();

	const char *const command = args.shift();

	if (StringIsEqual(command, "nop")) {
		Nop(server, args);
		return EXIT_SUCCESS;
	} else if (StringIsEqual(command, "verbose")) {
		Verbose(server, args);
		return EXIT_SUCCESS;
	} else if (StringIsEqual(command, "disable-queue")) {
		DisableQueue(server, args);
	} else if (StringIsEqual(command, "enable-queue")) {
		EnableQueue(server, args);
	} else if (StringIsEqual(command, "terminate-children")) {
		TerminateChildren(server, args);
	} else
		throw Usage{"Unknown command"};
} catch (const Usage &u) {
	if (u.msg)
		fmt::print(stderr, "{}\n\n", u.msg);

	fmt::print(stderr, "Usage: {} [--server=SERVER[:PORT]] COMMAND ...\n"
		   "\n"
		   "Commands:\n"
		   "  verbose LEVEL\n"
		   "  disable-queue\n"
		   "  enable-queue\n"
		   "  terminate-children TAG\n"
		   "  nop\n",
		   argv[0]);
	return EXIT_FAILURE;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
