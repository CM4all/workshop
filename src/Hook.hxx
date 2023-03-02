// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
class UniqueSocketDescriptor;
class SpawnServerClient;
class CurlGlobal;
class MultiLibrary;
class ControlServer;
class WorkshopPartition;
class CronPartition;

class WorkshopSpawnHook final : public SpawnHook {
	MultiLibrary *const library;

public:
	WorkshopSpawnHook(MultiLibrary *_library) noexcept
		:library(_library) {}

	/* virtual methods from SpawnHook */
	bool Verify(const PreparedChildProcess &p) override;
};
