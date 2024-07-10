// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Operator.hxx"
#include "event/SocketEvent.hxx"

#include <array>
#include <cstdint>
#include <memory>

struct ChildOptions;
class SpawnService;
class ChildProcessHandle;
namespace Co { template<typename T> class Task; }

/**
 * A #CronJob which sends a HTTP GET request to a specific URL.
 */
class CronCurlOperator final
	: public CronOperator
{
	SocketEvent socket;

	std::unique_ptr<ChildProcessHandle> pid;

public:
	[[nodiscard]]
	CronCurlOperator(EventLoop &event_loop) noexcept;
	~CronCurlOperator() noexcept override;

	[[nodiscard]]
	Co::Task<void> Start(SpawnService &spawn_service, const char *name,
			     const ChildOptions &options, const char *url);

private:
	void OnSocketReady(unsigned events) noexcept;
};
