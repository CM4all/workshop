// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Operator.hxx"
#include "spawn/ExitListener.hxx"

#include <memory>
#include <string_view>

struct PreparedChildProcess;
class EventLoop;
class SpawnService;
class PipeCaptureBuffer;
class SocketDescriptor;
class ChildProcessHandle;
class LazyDomainLogger;

/**
 * A #CronJob being executed as a spawned child process.
 */
class CronSpawnOperator final
	: public CronOperator, ExitListener
{
	std::unique_ptr<ChildProcessHandle> pid;

	std::unique_ptr<PipeCaptureBuffer> output_capture;

	LazyDomainLogger &logger;

public:
	explicit CronSpawnOperator(LazyDomainLogger &_logger) noexcept;
	~CronSpawnOperator() noexcept override;

	void Spawn(EventLoop &event_loop, SpawnService &spawn_service,
		   const char *name, std::string_view site,
		   PreparedChildProcess &&p, SocketDescriptor pond_socket);

public:
	/* virtual methods from ExitListener */
	void OnChildProcessExit(int status) noexcept override;
};
