// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "pg/Config.hxx"
#include "net/LocalSocketAddress.hxx"
#include "config.h"

#ifdef HAVE_AVAHI
#include "lib/avahi/ServiceConfig.hxx"
#endif

#include <string>

struct WorkshopPartitionConfig {
	/**
	 * Partition name.  Empty when not specified.
	 */
	std::string name;

#ifdef HAVE_AVAHI
	Avahi::ServiceConfig zeroconf;
#endif

	Pg::Config database;

	LocalSocketAddress translation_socket;

	/**
	 * Partition tag for #TRANSLATE_LISTENER_TAG.  Empty when not
	 * specified.
	 */
	std::string tag;

	size_t max_log = 8192;

	bool enable_journal = false;

#ifdef HAVE_AVAHI
	bool sticky = false;
#endif

	explicit WorkshopPartitionConfig(std::string &&_name) noexcept
		:name(std::move(_name))
	{
		translation_socket.Clear();
	}

	void Check() const;

#ifdef HAVE_AVAHI
	[[gnu::pure]]
	bool UsesZeroconf() const noexcept {
		return sticky;
	}
#endif
};
