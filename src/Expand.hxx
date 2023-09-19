// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <map>
#include <string>

using StringMap = std::map<std::string_view, std::string_view, std::less<>>;

void
Expand(std::string &p, const StringMap &vars) noexcept;
