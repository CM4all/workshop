/*
 * Copyright 2006-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Partition.hxx"
#include "Job.hxx"
#include "../Config.hxx"
#include "EmailService.hxx"
#include "net/ConnectSocket.hxx"

CronPartition::CronPartition(EventLoop &event_loop,
			     SpawnService &_spawn_service,
			     CurlGlobal &_curl,
			     const Config &root_config,
			     const CronPartitionConfig &config,
			     BoundMethod<void()> _idle_callback)
	:name(config.name.empty() ? nullptr : config.name.c_str()),
	 tag(config.tag.empty() ? nullptr : config.tag.c_str()),
	 translation_socket(config.translation_socket.c_str()),
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
	 workplace(_spawn_service, email_service.get(), pond_socket,
			   _curl, *this,
			   root_config.concurrency),
	 idle_callback(_idle_callback)
{
}

CronPartition::~CronPartition()
{
}

void
CronPartition::BeginShutdown()
{
	queue.DisableAdmin();
	workplace.CancelAll();

	if (email_service)
		email_service->CancelAll();
}

void
CronPartition::OnJob(CronJob &&job)
{
	logger(4, "OnJob ", job.id);

	if (!queue.Claim(job))
		return;

	try {
		workplace.Start(queue, translation_socket,
				name, tag,
				std::move(job));
	} catch (const std::runtime_error &e) {
		logger(1, "failed to start cronjob '", job.id, "': ",
		       std::current_exception());
	}

	if (workplace.IsFull())
		queue.DisableFull();
}

void
CronPartition::OnChildProcessExit(int)
{
	if (!workplace.IsFull())
		queue.EnableFull();

	if (IsIdle())
		idle_callback();
}
