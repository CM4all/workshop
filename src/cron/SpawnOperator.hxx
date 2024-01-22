// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef CRON_SPAWN_OPERATOR_HXX
#define CRON_SPAWN_OPERATOR_HXX

#include "Operator.hxx"
#include "spawn/ExitListener.hxx"

#include <memory>

struct PreparedChildProcess;
class SpawnService;
class PipeCaptureBuffer;
class SocketDescriptor;
class ChildProcessHandle;

/**
 * A #CronJob being executed as a spawned child process.
 */
class CronSpawnOperator final
	: public CronOperator, ExitListener
{
	SpawnService &spawn_service;

	std::unique_ptr<ChildProcessHandle> pid;

	std::unique_ptr<PipeCaptureBuffer> output_capture;

public:
	CronSpawnOperator(EventLoop &event_loop, CronHandler &_handler,
			  SpawnService &_spawn_service,
			  CronJob &&_job,
			  std::string_view _tag) noexcept;
	~CronSpawnOperator() noexcept override;

	void Spawn(PreparedChildProcess &&p, SocketDescriptor pond_socket);

public:
	/* virtual methods from ExitListener */
	void OnChildProcessExit(int status) noexcept override;
};

#endif
