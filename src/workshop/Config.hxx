// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/LocalSocketAddress.hxx"

#include <string>

struct WorkshopPartitionConfig {
	std::string database, database_schema;

	LocalSocketAddress translation_socket;

	/**
	 * Partition tag for #TRANSLATE_LISTENER_TAG.  Empty when not
	 * specified.
	 */
	std::string tag;

	size_t max_log = 8192;

	bool enable_journal = false;

	WorkshopPartitionConfig() noexcept {
		translation_socket.Clear();
	}

	void Check() const;
};
