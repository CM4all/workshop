// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/AllocatedSocketAddress.hxx"

#include <string>

struct WorkshopPartitionConfig {
	std::string database, database_schema;

	AllocatedSocketAddress translation_socket;

	/**
	 * Partition tag for #TRANSLATE_LISTENER_TAG.  Empty when not
	 * specified.
	 */
	std::string tag;

	size_t max_log = 8192;

	bool enable_journal = false;

	WorkshopPartitionConfig() = default;
	explicit WorkshopPartitionConfig(const char *_database)
		:database(_database) {}

	void Check() const;
};
