// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Workplace.hxx"
#include "Operator.hxx"
#include "Plan.hxx"
#include "Job.hxx"
#include "pg/Array.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/CgroupOptions.hxx"
#include "spawn/Interface.hxx"
#include "spawn/Client.hxx"
#include "net/EasyMessage.hxx"
#include "net/SocketPair.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/Pipe.hxx"
#include "util/DeleteDisposer.hxx"

#include <cassert>
#include <string>
#include <map>
#include <set>
#include <list>
#include <tuple> // for std::tie()

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

	auto [stderr_r, stderr_w] = CreatePipe();
	stderr_r.SetNonBlocking();

	/* create operator object */

	UniqueFileDescriptor stderr_w_for_operator =
		plan->control_channel && plan->allow_spawn
		? stderr_w.Duplicate()
		: UniqueFileDescriptor{};

	auto o = std::make_unique<WorkshopOperator>(event_loop, *this, job, std::move(plan),
						    std::move(stderr_r),
						    std::move(stderr_w_for_operator),
						    max_log,
						    enable_journal);
	o->Start(stderr_w);

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
