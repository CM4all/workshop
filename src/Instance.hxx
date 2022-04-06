/*
 * Copyright 2006-2021 CM4all GmbH
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

#pragma once

#include "control/Handler.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "event/DeferEvent.hxx"
#include "event/systemd/Watchdog.hxx"
#include "spawn/Registry.hxx"
#include "spawn/Hook.hxx"
#include "lib/curl/Init.hxx"
#include "lib/curl/Global.hxx"
#include "io/Logger.hxx"

#include <forward_list>

struct Config;
class SpawnServerClient;
class CurlGlobal;
class MultiLibrary;
class ControlServer;
class WorkshopPartition;
class CronPartition;

class Instance final : SpawnHook, ControlHandler {
	const RootLogger logger;

	EventLoop event_loop;

	Systemd::Watchdog systemd_watchdog{event_loop};

	bool should_exit = false;

	ShutdownListener shutdown_listener;
	SignalEvent sighup_event;
	DeferEvent defer_idle_check;

	ChildProcessRegistry child_process_registry;

	std::unique_ptr<SpawnServerClient> spawn_service;

	ScopeCurlInit curl_init;
	CurlGlobal curl;

	std::unique_ptr<MultiLibrary> library;

	std::forward_list<WorkshopPartition> partitions;

	std::forward_list<CronPartition> cron_partitions;

	std::forward_list<ControlServer> control_servers;

public:
	explicit Instance(const Config &config);

	~Instance() noexcept;

	EventLoop &GetEventLoop() noexcept {
		return event_loop;
	}

	void Start();

	void Dispatch() noexcept {
		event_loop.Dispatch();
	}

	void UpdateFilter(bool library_modified) noexcept;
	void UpdateLibraryAndFilter(bool force) noexcept;

private:
	void OnExit() noexcept;
	void OnReload(int) noexcept;

	void OnPartitionIdle() noexcept;
	void RemoveIdlePartitions() noexcept;

	/* virtual methods from SpawnHook */
	bool Verify(const PreparedChildProcess &p) override;

	/* virtual methods from ControlHandler */
	void OnControlPacket(WorkshopControlCommand command,
			     ConstBuffer<void> payload) override;
	void OnControlError(std::exception_ptr ep) noexcept override;
};
