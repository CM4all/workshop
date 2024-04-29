// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Partition.hxx"
#include "Job.hxx"
#include "../Config.hxx"
#include "EmailService.hxx"
#include "net/ConnectSocket.hxx"

CronPartition::CronPartition(EventLoop &event_loop,
			     SpawnService &_spawn_service,
			     const Config &root_config,
			     const CronPartitionConfig &config,
			     BoundMethod<void() noexcept> _idle_callback)
	:name(config.name),
	 tag(config.tag.empty() ? nullptr : config.tag.c_str()),
	 translation_socket(config.translation_socket),
	 logger("cron/" + config.name),
	 email_service(config.qmqp_server.IsNull()
		       ? nullptr
		       : new EmailService(event_loop, config.qmqp_server)),
	 pond_socket(!config.pond_server.IsNull()
	 ? CreateConnectDatagramSocket(config.pond_server)
	 : UniqueSocketDescriptor()),
	 queue(logger, event_loop, root_config.node_name.c_str(),
		       config.database.c_str(), config.database_schema.c_str(),
		       [this](CronJob &&job){ OnJob(std::move(job)); }),
	 workplace(_spawn_service,
		   email_service.get(), config.default_email_sender,
		   pond_socket,
		   *this,
		   root_config.concurrency),
	 idle_callback(_idle_callback),
	 default_timeout(config.default_timeout)
{
}

CronPartition::~CronPartition() noexcept = default;

void
CronPartition::BeginShutdown() noexcept
{
	queue.DisableAdmin();
	workplace.CancelAll();

	if (email_service)
		email_service->CancelAll();
}

void
CronPartition::OnJob(CronJob &&job) noexcept
{
	logger(4, "OnJob ", job.id);

	if (job.timeout.count() <= 0)
		job.timeout = default_timeout;

	if (!queue.Claim(job))
		return;

	try {
		workplace.Start(queue, translation_socket,
				name, tag,
				std::move(job));
	} catch (...) {
		logger(1, "failed to start cronjob '", job.id, "': ",
		       std::current_exception());
	}

	if (workplace.IsFull())
		queue.DisableFull();
}

void
CronPartition::OnChildProcessExit(int) noexcept
{
	if (!workplace.IsFull())
		queue.EnableFull();

	if (IsIdle())
		idle_callback();
}
