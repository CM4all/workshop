// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CurlOperator.hxx"
#include "Result.hxx"
#include "CaptureBuffer.hxx"
#include "lib/curl/Adapter.hxx"
#include "lib/curl/Easy.hxx"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Setup.hxx"
#include "spawn/ChildOptions.hxx"
#include "spawn/CoEnqueue.hxx"
#include "spawn/CoWaitSpawnCompletion.hxx"
#include "spawn/Interface.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/ProcessHandle.hxx"
#include "net/SocketError.hxx"
#include "net/SocketPair.hxx"
#include "net/SocketProtocolError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "co/Task.hxx"
#include "util/SpanCast.hxx"
#include "util/StringCompare.hxx"

using std::string_view_literals::operator""sv;

namespace {

class MyResponseHandler final : public CurlResponseHandler {
	const SocketDescriptor socket;
	std::exception_ptr error;
	bool is_text = false;

public:
	explicit MyResponseHandler(SocketDescriptor _socket) noexcept
		:socket(_socket) {}

	void CheckRethrowError() {
		if (error)
			std::rethrow_exception(error);
	}

	void OnHeaders(HttpStatus status, Curl::Headers &&headers) override {
		(void)socket.Write(ReferenceAsBytes(status));

		if (const auto i = headers.find("content-type"sv);
		    i != headers.end())
			is_text = i->second.starts_with("text/"sv);
	}

	void OnData(std::span<const std::byte> data) override {
		if (!is_text)
			return;

		if (socket.Write(data) < 0)
			throw MakeSocketError("send() failed");
	}

	void OnEnd() override {}

	void OnError(std::exception_ptr _error) noexcept override {
		error = std::move(_error);
	}
};

static CurlEasy
ReadRequest(SocketDescriptor s)
{
	char url[4096];

	if (const auto nbytes = s.Receive(std::as_writable_bytes(std::span{url}));
	    nbytes < 0)
		throw MakeSocketError("recvmsg() failed");
	else if (static_cast<std::size_t>(nbytes) == sizeof(url))
		throw SocketBufferFullError{};
	else
		url[nbytes] = 0;

	return CurlEasy{url};
}

static int
SpawnCurlFunction(PreparedChildProcess &&)
{
	SocketDescriptor control{3};

	auto easy = ReadRequest(control);
	Curl::Setup(easy);

	MyResponseHandler handler{control};
	CurlResponseHandlerAdapter adapter{handler};
	adapter.Install(easy);

	easy.Perform();
	adapter.Done(CURLE_OK);

	handler.CheckRethrowError();

	return 0;
}

static std::pair<UniqueSocketDescriptor, std::unique_ptr<ChildProcessHandle>>
SpawnCurl(SpawnService &spawn_service, const char *name,
	  const ChildOptions &options)
{
	// TODO this is a horrible and inefficient kludge
	auto [control_socket, control_socket_for_child] = CreateSocketPair(SOCK_SEQPACKET);

	PreparedChildProcess p;
	p.exec_function = SpawnCurlFunction;
	p.args.push_back("dummy");
	p.ns = {ShallowCopy{}, options.ns};
	p.ns.enable_pid = false;
	p.ns.enable_cgroup = false;
	p.ns.enable_ipc = false;
	p.ns.pid_namespace = nullptr;
	p.ns.mount.mount_proc = false;
	p.ns.mount.mount_dev = false;
	p.ns.mount.mount_pts = false;
	p.ns.mount.bind_mount_pts = false;
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

} // anonymous namespace

CronCurlOperator::CronCurlOperator(EventLoop &event_loop) noexcept
	:socket(event_loop, BIND_THIS_METHOD(OnSocketReady))
{
}

CronCurlOperator::~CronCurlOperator() noexcept
{
	socket.Close();
}

Co::Task<void>
CronCurlOperator::Start(SpawnService &spawn_service, const char *name,
			const ChildOptions &options, const char *url)
{
	co_await CoEnqueueSpawner{spawn_service};

	auto [_socket, _pid] = SpawnCurl(spawn_service, name, options);

	co_await CoWaitSpawnCompletion{*_pid};

	if (_socket.Write(AsBytes(std::string_view{url})) < 0)
		throw MakeSocketError("Failed to send");

	pid = std::move(_pid);

	socket.Open(_socket.Release());
	socket.ScheduleRead();
}

void
CronCurlOperator::OnSocketReady(unsigned) noexcept
{
	if (status == HttpStatus{}) {
		if (socket.GetSocket().ReadNoWait(ReferenceAsWritableBytes(status)) != static_cast<ssize_t>(sizeof(status)) ||
		    status == HttpStatus{})
			Finish(CronResult::Error("Failed to read status"));

		return;
	}

	auto w = capture.Write();
	const auto nbytes = socket.GetSocket().ReadNoWait(std::as_writable_bytes(w));
	if (nbytes > 0) {
		capture.Append(nbytes);
	} else {
		Finish(CronResult{
				.log = std::move(capture).NormalizeASCII(),
				.exit_status = static_cast<int>(status),
			});
	}
}
