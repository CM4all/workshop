// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/Chrono.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/LocalSocketAddress.hxx"

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

	LocalSocketAddress translation_socket;

	AllocatedSocketAddress qmqp_server;

	std::string default_email_sender{"cm4all-workshop"};

	/**
	 * The Pond server to receive the output of job processes.
	 */
	AllocatedSocketAddress pond_server;

	Event::Duration default_timeout = std::chrono::minutes{5};

	bool use_qrelay = false;

	explicit CronPartitionConfig(std::string &&_name):name(std::move(_name)) {
		translation_socket.Clear();
	}

	void Check() const;
};
