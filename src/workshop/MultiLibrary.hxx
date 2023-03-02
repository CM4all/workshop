// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef WORKSHOP_MULTI_LIBRARY_HXX
#define WORKSHOP_MULTI_LIBRARY_HXX

#include "Library.hxx"

#include <memory>
#include <forward_list>

/**
 * Manages several #Library instances as one.
 */
class MultiLibrary {
	std::forward_list<Library> libraries;

public:
	void InsertPath(std::filesystem::path &&_path) {
		libraries.emplace_front(std::move(_path));
	}

	bool Update(std::chrono::steady_clock::time_point now, bool force) {
		bool modified = false;
		for (auto &i : libraries)
			if (i.Update(now, force))
				modified = true;
		return modified;
	}

	template<typename F>
	void VisitAvailable(std::chrono::steady_clock::time_point now,
			    F &&f) const {
		for (const auto &i : libraries)
			i.VisitAvailable(now, f);
	}

	std::shared_ptr<Plan> Get(std::chrono::steady_clock::time_point now,
				  const char *name);
};

#endif
