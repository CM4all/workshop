// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Workplace.hxx"
#include "Operator.hxx"
#include "debug.h"
#include "Plan.hxx"
#include "Job.hxx"
#include "pg/Array.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/CgroupOptions.hxx"
#include "spawn/Interface.hxx"
#include "spawn/Client.hxx"
#include "system/Error.hxx"
#include "net/EasyMessage.hxx"
#include "net/SocketError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/StringCompare.hxx"

#include <cassert>
#include <string>
#include <map>
#include <set>
#include <list>

#include <sys/socket.h>

WorkshopWorkplace::WorkshopWorkplace(SpawnService &_spawn_service,
				     ExitListener &_exit_listener,
				     const Logger &parent_logger,
				     const char *_node_name,
				     SocketAddress _translation_socket,
				     const char *_listener_tag,
				     std::size_t _max_operators,
				     bool _enable_journal) noexcept
	:spawn_service(_spawn_service), exit_listener(_exit_listener),
	 logger(parent_logger, "workplace"),
	 node_name(_node_name),
	 translation_socket(_translation_socket),
	 listener_tag(_listener_tag),
	 max_operators(_max_operators),
	 enable_journal(_enable_journal)
{
	assert(max_operators > 0);
}

WorkshopWorkplace::~WorkshopWorkplace() noexcept
{
	assert(operators.empty());
}

std::string
WorkshopWorkplace::GetRunningPlanNames() const noexcept
{
	std::set<std::string_view, std::less<>> list;
	for (const auto &o : operators)
		list.emplace(o.GetPlanName());

	return Pg::EncodeArray(list);
}

std::string
WorkshopWorkplace::GetFullPlanNames() const noexcept
{
	std::map<std::string_view, std::size_t, std::less<>> counters;
	std::set<std::string_view, std::less<>> list;
	for (const auto &o : operators) {
		const Plan &plan = o.GetPlan();
		if (plan.concurrency == 0)
			continue;

		const std::string_view plan_name = o.GetPlanName();

		auto i = counters.emplace(plan_name, 0);
		std::size_t &n = i.first->second;

		++n;

		if (n >= plan.concurrency)
			list.emplace(plan_name);
	}

	return Pg::EncodeArray(list);
}

void
WorkshopWorkplace::Start(EventLoop &event_loop, const WorkshopJob &job,
			 std::shared_ptr<Plan> plan,
			 size_t max_log)
{
	assert(!plan->args.empty());

	/* create stdout/stderr pipes */

	UniqueFileDescriptor stderr_r, stderr_w;
	if (!UniqueFileDescriptor::CreatePipe(stderr_r, stderr_w))
		throw MakeErrno("pipe() failed");

	stderr_r.SetNonBlocking();

	/* create control socket */

	UniqueSocketDescriptor control_parent, control_child;
	if (plan->control_channel) {
		if (!UniqueSocketDescriptor::CreateSocketPair(AF_LOCAL, SOCK_SEQPACKET, 0,
							      control_parent, control_child))
			throw MakeErrno("socketpair() failed");

		control_parent.SetNonBlocking();
	}

	/* create operator object */

	UniqueFileDescriptor stderr_w_for_operator =
		plan->control_channel && plan->allow_spawn
		? stderr_w.Duplicate()
		: UniqueFileDescriptor{};

	auto o = std::make_unique<WorkshopOperator>(event_loop, *this, job, plan,
						    std::move(stderr_r),
						    std::move(stderr_w_for_operator),
						    std::move(control_parent),
						    max_log,
						    enable_journal);

	PreparedChildProcess p;
	p.hook_info = job.plan_name.c_str();
	p.SetStderr(std::move(stderr_w));

	if (control_child.IsDefined())
		p.SetControl(std::move(control_child));

	if (!debug_mode) {
		p.uid_gid.uid = plan->uid;
		p.uid_gid.gid = plan->gid;

		std::copy(plan->groups.begin(), plan->groups.end(),
			  p.uid_gid.groups.begin());
	}

	if (!plan->chroot.empty())
		p.chroot = plan->chroot.c_str();

	p.umask = plan->umask;
	p.rlimits = plan->rlimits;
	p.priority = plan->priority;
	p.sched_idle = plan->sched_idle;
	p.ioprio_idle = plan->ioprio_idle;
	p.ns.enable_network = plan->private_network;

	if (plan->private_tmp)
		p.ns.mount.mount_tmp_tmpfs = "";

	p.no_new_privs = true;

	/* use a per-plan cgroup */

	CgroupOptions cgroup;
	p.cgroup = &cgroup;

	UniqueSocketDescriptor return_cgroup;

	if (auto *client = dynamic_cast<SpawnServerClient *>(&spawn_service)) {
		if (client->SupportsCgroups()) {
			cgroup.name = job.plan_name.c_str();

			if (!UniqueSocketDescriptor::CreateSocketPair(AF_LOCAL, SOCK_DGRAM, 0,
								      return_cgroup,
								      p.return_cgroup))
				throw MakeSocketError("socketpair() failed");
		}
	}

	/* create stdout/stderr pipes */

	if (plan->control_channel) {
		/* copy stdout to stderr into the "log" column */
		p.stdout_fd = p.stderr_fd;
	} else {
		/* if there is no control channel, read progress from the
		   stdout pipe */
		UniqueFileDescriptor stdout_r, stdout_w;
		if (!UniqueFileDescriptor::CreatePipe(stdout_r, stdout_w))
			throw MakeErrno("pipe() failed");

		o->SetOutput(std::move(stdout_r));
		p.SetStdout(std::move(stdout_w));
	}

	/* build command line */

	std::list<std::string> args;
	args.insert(args.end(), plan->args.begin(), plan->args.end());
	args.insert(args.end(), job.args.begin(), job.args.end());

	o->Expand(args);

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

	o->SetPid(spawn_service.SpawnChildProcess(job.id.c_str(),
						  std::move(p)));

	logger(2, "job ", job.id, " (plan '", job.plan_name,
	       "') started");

	try {
		o->SetCgroup(EasyReceiveMessageWithOneFD(return_cgroup));
	} catch (...) {
		logger(1, "Failed to receive cgroup fd: ",
		       std::current_exception());
	}

	operators.push_back(*o.release());
}

void
WorkshopWorkplace::OnExit(WorkshopOperator *o) noexcept
{
	operators.erase_and_dispose(operators.iterator_to(*o),
				    DeleteDisposer{});

	exit_listener.OnChildProcessExit(-1);
}

void
WorkshopWorkplace::OnTimeout(WorkshopOperator *o) noexcept
{
	OnExit(o);
}
