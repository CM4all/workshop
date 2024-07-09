// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Operator.hxx"
#include "event/SocketEvent.hxx"
#include "CaptureBuffer.hxx"

#include <cstdint>
#include <memory>

enum class HttpStatus : uint_least16_t;
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

	CaptureBuffer capture{8192};

	HttpStatus status{};

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
