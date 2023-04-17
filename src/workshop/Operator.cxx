// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Operator.hxx"
#include "ProgressReader.hxx"
#include "ControlChannelServer.hxx"
#include "Expand.hxx"
#include "Workplace.hxx"
#include "Plan.hxx"
#include "Job.hxx"
#include "LogBridge.hxx"
#include "translation/Response.hxx"
#include "translation/SpawnClient.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "spawn/Interface.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/ProcessHandle.hxx"
#include "net/ConnectSocket.hxx"
#include "net/EasyMessage.hxx"
#include "net/SocketError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/Open.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/UTF8.hxx"
#include "AllocatorPtr.hxx"
#include "CgroupAccounting.hxx"

#include <fmt/core.h>

#include <tuple>

#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>

using std::string_view_literals::operator""sv;

class WorkshopOperator::SpawnedProcess final
	: ExitListener, public AutoUnlinkIntrusiveListHook
{
	std::unique_ptr<ChildProcessHandle> handle;

public:
	SpawnedProcess(std::unique_ptr<ChildProcessHandle> &&_handle) noexcept
		:handle(std::move(_handle))
	{
		handle->SetExitListener(*this);
	}

private:
	/* virtual methods from ExitListener */
	void OnChildProcessExit(int) noexcept override {
		handle.reset();

		// TODO remove this from linked list
	}
};

WorkshopOperator::WorkshopOperator(EventLoop &_event_loop,
				   WorkshopWorkplace &_workplace,
				   const WorkshopJob &_job,
				   const std::shared_ptr<Plan> &_plan,
				   UniqueFileDescriptor stderr_read_pipe,
				   UniqueFileDescriptor _stderr_write_pipe,
				   UniqueSocketDescriptor control_socket,
				   size_t max_log_buffer,
				   bool enable_journal) noexcept
	:event_loop(_event_loop),
	 workplace(_workplace), job(_job), plan(_plan),
	 logger(*this),
	 timeout_event(event_loop, BIND_THIS_METHOD(OnTimeout)),
	 control_channel(control_socket.IsDefined()
			 ? new WorkshopControlChannelServer(event_loop,
							    std::move(control_socket),
							    *this)
			 : nullptr),
	 stderr_write_pipe(std::move(_stderr_write_pipe)),
	 log(event_loop, job.plan_name.c_str(), job.id.c_str(),
	     std::move(stderr_read_pipe))
{
	ScheduleTimeout();

	if (max_log_buffer > 0)
		log.EnableBuffer(max_log_buffer);

	if (enable_journal)
		log.EnableJournal();
}

WorkshopOperator::~WorkshopOperator() noexcept
{
	children.clear_and_dispose(DeleteDisposer{});

	timeout_event.Cancel();
}

void
WorkshopOperator::ScheduleTimeout() noexcept
{
	const auto t = plan->parsed_timeout;
	if (t > t.zero())
		timeout_event.Schedule(t);
}

void
WorkshopOperator::OnTimeout() noexcept
{
	logger(2, "timed out; sending SIGTERM");

	job.SetDone(-1, "Timeout");

	workplace.OnTimeout(this);
}

void
WorkshopOperator::OnProgress(unsigned progress) noexcept
{
	if (exited)
		/* after the child process has exited, it's pointless to
		   update the progress because it will be set to 100% anyway;
		   this state can occur in OnChildProcessExit() during the
		   LogBridge::Flush() call */
		return;

	job.SetProgress(progress, plan->timeout.c_str());

	/* refresh the timeout */
	ScheduleTimeout();
}

void
WorkshopOperator::SetCgroup(FileDescriptor fd) noexcept
{
	if (cgroup_cpu_stat.OpenReadOnly(fd, "cpu.stat")) {
		try {
			cpu_usage_start = ReadCgroupCpuUsage(cgroup_cpu_stat);
		} catch (...) {
			logger(1, "Failed to read CPU usage: ",
			       std::current_exception());
		}
	}
}

void
WorkshopOperator::SetOutput(UniqueFileDescriptor fd) noexcept
{
	assert(fd.IsDefined());
	assert(!progress_reader);

	progress_reader.reset(new ProgressReader(event_loop, std::move(fd),
						 BIND_THIS_METHOD(OnProgress)));

}

void
WorkshopOperator::Expand(std::list<std::string> &args) const noexcept
{
	assert(!args.empty());

	StringMap vars;
	vars.emplace("0"sv, args.front());
	vars.emplace("NODE"sv, workplace.GetNodeName());
	vars.emplace("JOB"sv, job.id);
	vars.emplace("PLAN"sv, job.plan_name);

	for (auto &i : args)
		::Expand(i, vars);
}

void
WorkshopOperator::OnChildProcessExit(int status) noexcept
{
	exited = true;

	if (control_channel) {
		try {
			control_channel->ReceiveAll();
		} catch (...) {
			/* ignore control channel errors, the process
			   is gone anyway */
		}
	}

	log.Flush();

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

	const char *log_text = log.GetBuffer();
	if (log_text != nullptr && !ValidateUTF8(log_text)) {
		/* TODO: purge illegal UTF-8 sequences instead of
		   replacing the log text? */
		log_text = "Invalid UTF-8 output";
		logger(2, log_text);
	}

	if (cpu_usage_start.count() >= 0) {
		assert(cgroup_cpu_stat.IsDefined());

		try {
			const auto cpu_usage_end = ReadCgroupCpuUsage(cgroup_cpu_stat);
			if (cpu_usage_end.count() >= 0)
				job.AddCpuUsage(cpu_usage_end - cpu_usage_start);
		} catch (...) {
			logger(1, "Failed to read CPU usage: ",
			       std::current_exception());
		}
	}

	if (again >= std::chrono::seconds())
		job.SetAgain(again, log_text);
	else
		job.SetDone(exit_status, log_text);

	workplace.OnExit(this);
}

std::string
WorkshopOperator::MakeLoggerDomain() const noexcept
{
	return fmt::format("job {} plan={}", job.id, job.plan_name);
}

void
WorkshopOperator::OnControlProgress(unsigned progress) noexcept
{
	OnProgress(progress);
}

void
WorkshopOperator::OnControlSetEnv(const char *s) noexcept
{
	try {
		job.SetEnv(s);
	} catch (...) {
		logger(1, "Failed to 'setenv': ", std::current_exception());
	}
}

void
WorkshopOperator::OnControlAgain(std::chrono::seconds d) noexcept
{
	again = d;
}

static UniqueSocketDescriptor
CreateConnectBlockingSocket(const SocketAddress address, int type)
{
	auto s = CreateConnectSocket(address, type);
	s.SetBlocking();
	return s;
}

static std::pair<std::unique_ptr<ChildProcessHandle>, UniqueSocketDescriptor>
DoSpawn(SpawnService &service, AllocatorPtr alloc,
	const WorkshopJob &job, const Plan &plan,
	const char *token,
	FileDescriptor stderr_w,
	const TranslateResponse &response)
{
	if (response.status != HttpStatus{}) {
		if (response.message != nullptr)
			throw FmtRuntimeError("Status {} from translation server: {}",
					      static_cast<unsigned>(response.status),
					      response.message);

		throw FmtRuntimeError("Status {} from translation server",
				      static_cast<unsigned>(response.status));
	}

	if (response.execute == nullptr)
		throw std::runtime_error("No EXECUTE from translation server");

	if (response.child_options.uid_gid.IsEmpty())
		throw std::runtime_error("No UID_GID from translation server");

	PreparedChildProcess p;
	p.args.push_back(alloc.Dup(response.execute));

	if (stderr_w.IsDefined())
		p.stderr_fd = p.stdout_fd = FileDescriptor{dup(stderr_w.Get())};

	UniqueSocketDescriptor return_pidfd;
	if (!UniqueSocketDescriptor::CreateSocketPair(AF_LOCAL, SOCK_DGRAM, 0,
						      return_pidfd,
						      p.return_pidfd))
		throw MakeSocketError("socketpair() failed");

	for (const char *arg : response.args) {
		if (p.args.size() >= 4096)
			throw std::runtime_error("Too many APPEND packets from translation server");

		p.args.push_back(alloc.Dup(arg));
	}

	response.child_options.CopyTo(p);

	if (p.umask == -1)
		p.umask = plan.umask;

	p.priority = plan.priority;
	p.sched_idle = plan.sched_idle;
	p.ioprio_idle = plan.ioprio_idle;
	p.no_new_privs = true;

	/* use the same per-plan cgroup as the orignal job process */

	CgroupOptions cgroup;
	cgroup.name = job.plan_name.c_str();

	p.cgroup = &cgroup;

	return {
		service.SpawnChildProcess(token, std::move(p)),
		std::move(return_pidfd),
	};
}

UniqueFileDescriptor
WorkshopOperator::OnControlSpawn(const char *token, const char *param)
{
	if (!plan->allow_spawn)
		throw FmtRuntimeError("Plan '{}' does not have the 'allow_spawn' flag",
				      job.plan_name);

	const auto translation_socket = workplace.GetTranslationSocket();
	if (translation_socket == nullptr)
		throw std::runtime_error{"No 'translation_server' configured"};

	Allocator alloc;
	const auto response =
		TranslateSpawn(alloc,
			       CreateConnectBlockingSocket(translation_socket,
							   SOCK_STREAM),
			       workplace.GetListenerTag(),
			       job.plan_name.c_str(),
			       token, param);


	auto [handle, return_pidfd] =
		DoSpawn(workplace.GetSpawnService(), alloc, job, *plan,
			token, stderr_write_pipe, response);

	children.push_front(*new SpawnedProcess(std::move(handle)));

	return EasyReceiveMessageWithOneFD(return_pidfd);
}

void
WorkshopOperator::OnControlTemporaryError(std::exception_ptr ep) noexcept
{
	logger(3, "error on control channel: ", ep);
}

void
WorkshopOperator::OnControlPermanentError(std::exception_ptr ep) noexcept
{
	control_channel.reset();
	logger(3, "error on control channel: ", ep);
}

void
WorkshopOperator::OnControlClosed() noexcept
{
	control_channel.reset();
}
