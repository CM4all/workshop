// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "NsQrelayConnect.hxx"
#include "spawn/ChildOptions.hxx"
#include "spawn/ProcessHandle.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/Interface.hxx"
#include "net/LocalSocketAddress.hxx"
#include "net/ConnectSocket.hxx"
#include "net/SocketPair.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/Pipe.hxx"

#include <fcntl.h> // for splice()
#include <limits.h> // for INT_MAX
#include <poll.h>

using std::string_view_literals::operator""sv;

static std::size_t
SpliceToPipe(SocketDescriptor src, FileDescriptor dest) noexcept
{
	auto nbytes = splice(src.Get(), nullptr, dest.Get(), nullptr,
			     INT_MAX, SPLICE_F_MOVE|SPLICE_F_NONBLOCK);
	if (nbytes <= 0)
		return 0;

	return static_cast<std::size_t>(nbytes);
}

static bool
SpliceFromPipe(FileDescriptor src, SocketDescriptor dest, std::size_t size) noexcept
{
	while (size > 0) {
		auto nbytes = splice(src.Get(), nullptr, dest.Get(), nullptr,
				     size, SPLICE_F_MOVE);
		if (nbytes <= 0)
			return false;

		size -= static_cast<std::size_t>(nbytes);
	}

	return true;
}

static bool
SpliceSockets(SocketDescriptor src, SocketDescriptor dest,
	      FileDescriptor r, FileDescriptor w)
{
	std::size_t in_pipe = SpliceToPipe(src, w);
	return in_pipe > 0 && SpliceFromPipe(r, dest, in_pipe);
}

static void
SpliceTwoSockets(SocketDescriptor a, SocketDescriptor b)
{
	std::array fds{
		pollfd{.fd = a.Get(), .events = POLLIN},
		pollfd{.fd = b.Get(), .events = POLLIN},
	};

	const auto [r, w] = CreatePipe();

	do {
		if (poll(fds.data(), fds.size(), -1) <= 0)
			break;

		if (fds[0].revents && !SpliceSockets(a, b, r, w))
			fds[0].events = 0;

		if (fds[1].revents && !SpliceSockets(b, a, r, w))
			fds[1].events = 0;
	} while (fds[0].events && fds[1].events);
}

static int
NsConnectQrelayFunction(PreparedChildProcess &&)
{
	const SocketDescriptor control_socket{3};

	static constexpr LocalSocketAddress qrelay_address{"/run/cm4all/qrelay/socket"sv};
	const auto qrelay_socket = CreateConnectSocket(qrelay_address, SOCK_STREAM);

	/* now splice all data between the two sockets; it is
	   necessary that this containerized process does all the
	   network I/O to qrelay, or else qrelay can't recognize the
	   cient process and sees only the Workshop main process */
	SpliceTwoSockets(control_socket, qrelay_socket);

	return 0;
}

std::pair<UniqueSocketDescriptor, std::unique_ptr<ChildProcessHandle>>
NsConnectQrelay(SpawnService &spawn_service,
		const char *name, const ChildOptions &options)
{
	// TODO this is a horrible and inefficient kludge
	auto [control_socket, control_socket_for_child] = CreateSocketPair(SOCK_SEQPACKET);

	PreparedChildProcess p;
	p.exec_function = NsConnectQrelayFunction;
	p.args.push_back("dummy");
	p.cgroup = &options.cgroup;
	p.ns = {ShallowCopy{}, options.ns};
	p.ns.enable_pid = false;
	p.ns.enable_cgroup = false;
	p.ns.enable_ipc = false;
	p.ns.pid_namespace = nullptr;
	p.uid_gid = options.uid_gid;
#ifdef HAVE_LIBSECCOMP
	p.forbid_multicast = options.forbid_multicast;
	p.forbid_bind = options.forbid_bind;
#endif // HAVE_LIBSECCOMP
	p.control_fd = control_socket_for_child.ToFileDescriptor();

	return {
		std::move(control_socket),
		spawn_service.SpawnChildProcess(name, std::move(p)),
	};
}
