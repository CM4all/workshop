// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef WORKSHOP_PLAN_LOADER_HXX
#define WORKSHOP_PLAN_LOADER_HXX

#include <filesystem>

struct Plan;

/**
 * Parses plan configuration files.
 */
Plan
LoadPlanFile(const std::filesystem::path &path);

#endif
