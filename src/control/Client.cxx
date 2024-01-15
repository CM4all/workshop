// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Protocol.hxx"
#include "system/Error.hxx"
#include "io/Iovec.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/RConnectSocket.hxx"
#include "net/SendMessage.hxx"
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

class WorkshopControlClient {
	UniqueSocketDescriptor socket;

public:
	explicit WorkshopControlClient(UniqueSocketDescriptor _socket)
		:socket(std::move(_socket)) {}

	explicit WorkshopControlClient(const char *host_and_port)
		:WorkshopControlClient(ResolveConnectDatagramSocket(host_and_port,
								    WORKSHOP_CONTROL_DEFAULT_PORT)) {}

	void Send(WorkshopControlCommand cmd, std::span<const std::byte> payload={});
};

static constexpr size_t
PaddingSize(size_t size)
{
	return (3 - ((size - 1) & 0x3));
}

void
WorkshopControlClient::Send(WorkshopControlCommand cmd,
			    std::span<const std::byte> payload)
{
	WorkshopControlHeader h{ToBE16(payload.size()), ToBE16(uint16_t(cmd))};
	WorkshopControlDatagramHeader dh{ToBE32(WORKSHOP_CONTROL_MAGIC), 0};

	static constexpr std::byte padding[3]{};

	const struct iovec v[] = {
		MakeIovecT(dh),
		MakeIovecT(h),
		MakeIovec(payload),
		MakeIovec(std::span{padding, PaddingSize(payload.size())}),
	};

	CRC32State crc;
	for (size_t i = 1; i < ARRAY_SIZE(v); ++i)
		crc.Update({(const std::byte *)v[i].iov_base, v[i].iov_len});

	dh.crc = ToBE32(crc.Finish());

	SendMessage(socket, MessageHeader{v}, 0);
}

static void
SimpleCommand(const char *server, ConstBuffer<const char *> args,
	      WorkshopControlCommand cmd)
{
	if (!args.empty())
		throw Usage{"Too many arguments"};

	WorkshopControlClient client(server);
	client.Send(cmd);
}

static void
Nop(const char *server, ConstBuffer<const char *> args)
{
	SimpleCommand(server, args, WorkshopControlCommand::NOP);
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

	WorkshopControlClient client(server);
	client.Send(WorkshopControlCommand::VERBOSE,
		    ReferenceAsBytes(log_level));
}

static void
DisableQueue(const char *server, ConstBuffer<const char *> args)
{
	SimpleCommand(server, args, WorkshopControlCommand::DISABLE_QUEUE);
}

static void
EnableQueue(const char *server, ConstBuffer<const char *> args)
{
	SimpleCommand(server, args, WorkshopControlCommand::ENABLE_QUEUE);
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
		   "  nop\n",
		   argv[0]);
	return EXIT_FAILURE;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
