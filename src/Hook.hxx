// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "spawn/Hook.hxx"

class MultiLibrary;

class WorkshopSpawnHook final : public SpawnHook {
	MultiLibrary *const library;

public:
	WorkshopSpawnHook(MultiLibrary *_library) noexcept
		:library(_library) {}

	/* virtual methods from SpawnHook */
	bool Verify(const PreparedChildProcess &p) override;
};
