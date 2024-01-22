// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Operator.hxx"
#include "spawn/ExitListener.hxx"

#include <memory>

struct PreparedChildProcess;
class EventLoop;
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
	std::unique_ptr<ChildProcessHandle> pid;

	std::unique_ptr<PipeCaptureBuffer> output_capture;

public:
	CronSpawnOperator(const CronJob &_job, LazyDomainLogger &_logger) noexcept;
	~CronSpawnOperator() noexcept override;

	void Spawn(EventLoop &event_loop, SpawnService &spawn_service,
		   PreparedChildProcess &&p, SocketDescriptor pond_socket);

public:
	/* virtual methods from ExitListener */
	void OnChildProcessExit(int status) noexcept override;
};
