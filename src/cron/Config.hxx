// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/Chrono.hxx"
#include "net/AllocatedSocketAddress.hxx"

#include <string>

struct CronPartitionConfig {
	/**
	 * Partition name.  Empty when not specified.
	 */
	std::string name;

	/**
	 * Partition tag for #TRANSLATE_LISTENER_TAG.  Empty when not
	 * specified.
	 */
	std::string tag;

	std::string database, database_schema;

	AllocatedSocketAddress translation_socket;

	AllocatedSocketAddress qmqp_server;

	/**
	 * The Pond server to receive the output of job processes.
	 */
	AllocatedSocketAddress pond_server;

	Event::Duration default_timeout;

	explicit CronPartitionConfig(std::string &&_name):name(std::move(_name)) {}

	void Check() const;
};
