// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "system/Error.hxx"
#include "io/Iovec.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/control/Builder.hxx"
#include "net/control/Client.hxx"
#include "util/ByteOrder.hxx"
#include "util/CRC32.hxx"
#include "util/Macros.hxx"
#include "util/SpanCast.hxx"
#include "util/StringCompare.hxx"
#include "util/PrintException.hxx"

#include <fmt/core.h>

#include <span>

#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>

using namespace BengControl;

struct Usage {
	const char *msg = nullptr;
};

static void
SimpleCommand(const char *server, std::span<const char *const> args,
	      Command cmd)
{
	if (!args.empty())
		throw Usage{"Too many arguments"};

	Client client{server};
	client.Send(cmd);
}

static void
OptionalPayloadCommand(const char *server, std::span<const char *const> args,
		       Command cmd)
{
	Client client{server};

	Builder builder;

	if (args.empty())
		builder.Add(cmd);
	else
		for (const std::string_view i : args)
			builder.Add(cmd, i);

	client.Send(builder);
}

static void
Nop(const char *server, std::span<const char *const> args)
{
	SimpleCommand(server, args, Command::NOP);
}

static void
Verbose(const char *server, std::span<const char *const> args)
{
	if (args.empty())
		throw Usage{"Log level missing"};

	const char *s = args.front();
	args = args.subspan(1);

	if (!args.empty())
		throw Usage{"Too many arguments"};

	uint8_t log_level = atoi(s);

	Client client{server};
	client.Send(Command::VERBOSE,
		    ReferenceAsBytes(log_level));
}

static void
DisableQueue(const char *server, std::span<const char *const> args)
{
	OptionalPayloadCommand(server, args, Command::DISABLE_QUEUE);
}

static void
EnableQueue(const char *server, std::span<const char *const> args)
{
	OptionalPayloadCommand(server, args, Command::ENABLE_QUEUE);
}

static void
TerminateChildren(const char *server, std::span<const char *const> args)
{
	if (args.empty())
		throw Usage{"Tag missing"};

	Client client{server};

	Builder builder;
	for (const std::string_view tag : args)
		builder.Add(Command::TERMINATE_CHILDREN, tag);

	client.Send(builder);
}

int
main(int argc, char **argv)
try {
	std::span<const char *const> args{argv + 1, static_cast<std::size_t>(argc - 1)};

	const char *server = "@cm4all-workshop.control";

	while (!args.empty() && args.front()[0] == '-') {
		const char *option = args.front();
		args = args.subspan(1);
		if (const char *new_server = StringAfterPrefix(option, "--server=")) {
			server = new_server;
		} else
			throw Usage{"Unknown option"};
	}

	if (args.empty())
		throw Usage();

	const char *const command = args.front();
	args = args.subspan(1);

	if (StringIsEqual(command, "nop")) {
		Nop(server, args);
		return EXIT_SUCCESS;
	} else if (StringIsEqual(command, "verbose")) {
		Verbose(server, args);
		return EXIT_SUCCESS;
	} else if (StringIsEqual(command, "reload-state")) {
		SimpleCommand(server, args, Command::RELOAD_STATE);
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
		   "  reload-state\n"
		   "  disable-queue [NAME]\n"
		   "  enable-queue [NAME]\n"
		   "  terminate-children TAG\n"
		   "  nop\n",
		   argv[0]);
	return EXIT_FAILURE;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
