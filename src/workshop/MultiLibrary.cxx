// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "MultiLibrary.hxx"

std::shared_ptr<Plan>
MultiLibrary::Get(std::chrono::steady_clock::time_point now, const char *name)
{
	for (auto &i : libraries) {
		auto p = i.Get(now, name);
		if (p)
			return p;
	}

	return nullptr;
}
