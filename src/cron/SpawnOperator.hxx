// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Operator.hxx"
#include "spawn/ExitListener.hxx"

#include <memory>
#include <string_view>

struct PreparedChildProcess;
class AllocatorPtr;
class EventLoop;
class SpawnService;
class PipeCaptureBuffer;
class SocketDescriptor;
class ChildProcessHandle;
class LazyDomainLogger;
namespace Co { template<typename T> class Task; }

/**
 * A #CronJob being executed as a spawned child process.
 */
class CronSpawnOperator final
	: public CronOperator, ExitListener
{
	std::unique_ptr<ChildProcessHandle> pid;

	std::unique_ptr<PipeCaptureBuffer> output_capture;

	const LazyDomainLogger &logger;

public:
	[[nodiscard]]
	explicit CronSpawnOperator(LazyDomainLogger &_logger) noexcept;
	~CronSpawnOperator() noexcept override;

	[[nodiscard]]
	Co::Task<void> Spawn(EventLoop &event_loop, SpawnService &spawn_service,
			     AllocatorPtr alloc,
			     const char *name, std::string_view site,
			     PreparedChildProcess &&p, SocketDescriptor pond_socket);

public:
	/* virtual methods from ExitListener */
	void OnChildProcessExit(int status) noexcept override;
};
