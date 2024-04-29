// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <memory>
#include <utility>

struct ChildOptions;
class SpawnService;
class UniqueSocketDescriptor;
class ChildProcessHandle;

/**
 * Open a socket to qrelay from within the specified container.
 */
[[nodiscard]]
std::pair<UniqueSocketDescriptor, std::unique_ptr<ChildProcessHandle>>
NsConnectQrelay(SpawnService &spawn_service,
		const char *name, const ChildOptions &options);
