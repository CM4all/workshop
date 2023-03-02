// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef WORKSHOP_EXPAND_HXX
#define WORKSHOP_EXPAND_HXX

#include <map>
#include <string>

using StringMap = std::map<std::string_view, std::string_view, std::less<>>;

void
Expand(std::string &p, const StringMap &vars);

#endif
