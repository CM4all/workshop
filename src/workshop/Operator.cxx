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
#include "translation/ExecuteOptions.hxx"
#include "translation/SpawnClient.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "spawn/Client.hxx"
#include "spawn/CoEnqueue.hxx"
#include "spawn/CoWaitSpawnCompletion.hxx"
#include "spawn/Interface.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/ProcessHandle.hxx"
#include "net/ConnectSocket.hxx"
#include "net/EasyMessage.hxx"
#include "net/SocketPair.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/FdHolder.hxx"
#include "io/FileAt.hxx"
#include "io/Open.hxx"
#include "io/Pipe.hxx"
#include "co/Task.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/StringCompare.hxx"
#include "util/UTF8.hxx"
#include "AllocatorPtr.hxx"
#include "CgroupAccounting.hxx"
#include "debug.h"

#include <fmt/core.h>

#include <tuple>

#include <assert.h>
#include <unistd.h>
#include <string.h> // for strerror()
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
				   const std::shared_ptr<Plan> &_plan) noexcept
	:event_loop(_event_loop),
	 workplace(_workplace), job(_job), plan(_plan),
	 logger(*this),
	 timeout_event(event_loop, BIND_THIS_METHOD(OnTimeout))
{
	ScheduleTimeout();
}

WorkshopOperator::~WorkshopOperator() noexcept
{
	children.clear_and_dispose(DeleteDisposer{});
}

static void
PrepareChildProcess(PreparedChildProcess &p, const char *plan_name,
		    const Plan &plan,
		    FileDescriptor stderr_fd, SocketDescriptor control_fd)
{
	p.hook_info = plan_name;
	p.stderr_fd = p.stdout_fd = stderr_fd;
	p.control_fd = control_fd.ToFileDescriptor();

	if (!debug_mode) {
		p.uid_gid.effective_uid = plan.uid;
		p.uid_gid.effective_gid = plan.gid;

		std::copy(plan.groups.begin(), plan.groups.end(),
			  p.uid_gid.supplementary_groups.begin());
	}

	if (!plan.chroot.empty())
		p.chroot = plan.chroot.c_str();

	p.umask = plan.umask;
	p.rlimits = plan.rlimits;
	p.priority = plan.priority;
	p.sched_idle = plan.sched_idle;
	p.ioprio_idle = plan.ioprio_idle;
	p.ns.enable_network = plan.private_network;

	if (plan.private_tmp)
		p.ns.mount.mount_tmp_tmpfs = "";

	p.no_new_privs = true;
}

void
WorkshopOperator::Start(std::size_t max_log_buffer,
			bool enable_journal) noexcept
{
	assert(!task);

	task = Start2(max_log_buffer, enable_journal);
	task.Start(BIND_THIS_METHOD(OnTaskCompletion));
}

inline Co::InvokeTask
WorkshopOperator::Start2(std::size_t max_log_buffer,
			 bool enable_journal)
{
	assert(!pid);

	auto &spawn_service = workplace.GetSpawnService();

	co_await CoEnqueueSpawner{spawn_service};

	/* create stdout/stderr pipes */

	auto [stderr_r, stderr_w] = CreatePipe();
	stderr_r.SetNonBlocking();

	log.emplace(event_loop, job.plan_name, job.id, std::move(stderr_r));

	if (max_log_buffer > 0)
		log->EnableBuffer(max_log_buffer);

	if (enable_journal)
		log->EnableJournal();

	if (plan->control_channel && plan->allow_spawn)
		stderr_write_pipe = stderr_w.Duplicate();

	/* create control socket */

	UniqueSocketDescriptor control_child;
	if (plan->control_channel) {
		UniqueSocketDescriptor control_parent;
		std::tie(control_parent, control_child) = CreateSocketPair(SOCK_SEQPACKET);

		control_parent.SetNonBlocking();

		WorkshopControlChannelHandler &handler = *this;
		control_channel = std::make_unique<WorkshopControlChannelServer>(event_loop,
										 std::move(control_parent),
										 handler);
	}

	PreparedChildProcess p;
	PrepareChildProcess(p, job.plan_name.c_str(), *plan,
			    stderr_w, control_child);

	/* use a per-plan cgroup */

	CgroupOptions cgroup;

	UniqueSocketDescriptor return_cgroup;

	if (auto *client = dynamic_cast<SpawnServerClient *>(&spawn_service);
	    client != nullptr && client->SupportsCgroups()) {
		if (p.cgroup == nullptr) {
			cgroup.name = job.plan_name.c_str();
			p.cgroup = &cgroup;
		}

		p.cgroup_session = job.id.c_str();

		std::tie(return_cgroup, p.return_cgroup) = CreateSocketPair(SOCK_SEQPACKET);
	}

	/* create stdout/stderr pipes */

	UniqueFileDescriptor stdout_w;

	if (!plan->control_channel) {
		/* if there is no control channel, read progress from the
		   stdout pipe */
		UniqueFileDescriptor stdout_r;
		std::tie(stdout_r, stdout_w) = CreatePipe();

		SetOutput(std::move(stdout_r));
		p.stdout_fd = stdout_w;
	}

	/* build command line */

	std::list<std::string> args;
	args.insert(args.end(), plan->args.begin(), plan->args.end());
	args.insert(args.end(), job.args.begin(), job.args.end());

	Expand(args);

	for (const auto &i : args) {
		if (p.args.size() >= 4096)
			throw std::runtime_error("Too many command-line arguments");

		p.args.push_back(i.c_str());
	}

	for (const auto &i : job.env) {
		if (p.env.size() >= 64)
			throw std::runtime_error("Too many environment variables");

		if (StringStartsWith(i.c_str(), "LD_"))
			/* reject - too dangerous */
			continue;

		p.env.push_back(i.c_str());
	}

	/* fork */

	pid = spawn_service.SpawnChildProcess(job.id.c_str(), std::move(p));
	co_await CoWaitSpawnCompletion{*pid};

	pid->SetExitListener(*this);

	logger(2, "job ", job.id, " (plan '", job.plan_name,
	       "') started");

	if (return_cgroup.IsDefined()) {
		/* close the other side of the socketpair if it's
		   still open to avoid blocking the following receive
		   call if the spawner has closed the socket without
		   sending something */
		if (p.return_cgroup.IsDefined())
			p.return_cgroup.Close();

		try {
			SetCgroup(EasyReceiveMessageWithOneFD(return_cgroup));
		} catch (...) {
			logger(1, "Failed to receive cgroup fd: ",
			       std::current_exception());
		}
	}
}

inline void
WorkshopOperator::OnTaskCompletion(std::exception_ptr &&error) noexcept
{
	if (error) {
		logger.Fmt(1, "Failed to start job: {}"sv, std::move(error));
		workplace.OnExit(this);
	}
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
	if (cgroup_cpu_stat.OpenReadOnly({fd, "cpu.stat"})) {
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

	/* expand all parameters, but not the program name; this
	   wouldn't work because a pointer to it is in the "vars"
	   map */
	for (auto i = std::next(args.begin()); i != args.end(); ++i)
		::Expand(*i, vars);
}

void
WorkshopOperator::OnChildProcessExit(int status) noexcept
{
	assert(log);

	exited = true;

	if (control_channel) {
		try {
			control_channel->ReceiveAll();
		} catch (...) {
			/* ignore control channel errors, the process
			   is gone anyway */
		}
	}

	log->Flush();

	int exit_status = WEXITSTATUS(status);

	if (status < 0) {
		logger(2, "exited with errno ", strerror(-status));
		exit_status = status;
	} else if (WIFSIGNALED(status)) {
		logger(1, "died from signal ",
		       WTERMSIG(status),
		       WCOREDUMP(status) ? " (core dumped)" : "");
		exit_status = -1;
	} else if (exit_status == 0)
		logger(3, "exited with success");
	else
		logger(2, "exited with status ", exit_status);

	const char *log_text = log->GetBuffer();
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

	if (response.execute_options == nullptr ||
	    response.execute_options->execute == nullptr)
		throw std::runtime_error("No EXECUTE from translation server");

	const auto &options = *response.execute_options;

	if (options.child_options.uid_gid.IsEmpty() && !debug_mode)
		throw std::runtime_error("No UID_GID from translation server");

	FdHolder close_fds;
	PreparedChildProcess p;
	p.args.push_back(alloc.Dup(options.execute));

	if (stderr_w.IsDefined())
		p.stderr_fd = p.stdout_fd = stderr_w;

	UniqueSocketDescriptor return_pidfd;
	std::tie(return_pidfd, p.return_pidfd) = CreateSocketPair(SOCK_SEQPACKET);

	for (const char *arg : options.args) {
		if (p.args.size() >= 4096)
			throw std::runtime_error("Too many APPEND packets from translation server");

		p.args.push_back(alloc.Dup(arg));
	}

	options.child_options.CopyTo(p, close_fds);

	if (p.umask == -1)
		p.umask = plan.umask;

	p.priority = plan.priority;
	p.sched_idle = plan.sched_idle;
	p.ioprio_idle = plan.ioprio_idle;
	p.no_new_privs = true;

	/* use the same per-plan cgroup as the orignal job process */

	CgroupOptions cgroup;

	if (auto *client = dynamic_cast<SpawnServerClient *>(&service)) {
		if (client->SupportsCgroups()) {
			if (p.cgroup == nullptr || !p.cgroup->IsDefined()) {
				/* the translation server did not specify a cgroup -
				   fall back to the plan cgroup */
				p.cgroup = &cgroup;

				cgroup.name = job.plan_name.c_str();
			}

			p.cgroup_session = job.id.c_str();
		}
	}

	return {
		service.SpawnChildProcess(token, std::move(p)),
		std::move(return_pidfd),
	};
}

Co::Task<UniqueFileDescriptor>
WorkshopOperator::OnControlSpawn(const char *token, const char *param)
{
	if (!plan->allow_spawn)
		throw FmtRuntimeError("Plan '{}' does not have the 'allow_spawn' flag",
				      job.plan_name);

	const auto translation_socket = workplace.GetTranslationSocket();
	if (translation_socket == nullptr)
		throw std::runtime_error{"No 'translation_server' configured"};

	if (exited)
		throw std::runtime_error{"Main process has already exited"};

	Allocator alloc;
	const auto response = co_await
		TranslateSpawn(event_loop, alloc,
			       CreateConnectSocket(translation_socket, SOCK_STREAM),
			       workplace.GetListenerTag(),
			       job.plan_name.c_str(),
			       token, param);

	auto &spawn_service = workplace.GetSpawnService();

	co_await CoEnqueueSpawner{spawn_service};

	auto [handle, return_pidfd] =
		DoSpawn(spawn_service, alloc, job, *plan,
			token, stderr_write_pipe, response);

	co_await CoWaitSpawnCompletion{*handle};

	children.push_front(*new SpawnedProcess(std::move(handle)));

	co_return EasyReceiveMessageWithOneFD(return_pidfd);
}

void
WorkshopOperator::OnControlTemporaryError(std::exception_ptr &&error) noexcept
{
	logger(3, "error on control channel: ", std::move(error));
}

void
WorkshopOperator::OnControlPermanentError(std::exception_ptr &&error) noexcept
{
	control_channel.reset();
	logger(3, "error on control channel: ", std::move(error));
}

void
WorkshopOperator::OnControlClosed() noexcept
{
	control_channel.reset();
}
