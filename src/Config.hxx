// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "workshop/Config.hxx"
#include "cron/Config.hxx"
#include "spawn/Config.hxx"
#include "net/SocketConfig.hxx"

#include <string>
#include <forward_list>

struct Config {
	std::string node_name;
	unsigned concurrency = 2;

	SpawnConfig spawn;

	std::forward_list<WorkshopPartitionConfig> partitions;
	std::forward_list<CronPartitionConfig> cron_partitions;

	struct ControlListener : SocketConfig {
		ControlListener()
			:SocketConfig{
				.pass_cred = true,
			}
		{
		}

		explicit ControlListener(SocketAddress _bind_address)
			:SocketConfig{
				.bind_address = AllocatedSocketAddress{_bind_address},
				.pass_cred = true,
			}
		{
		}
	};

	std::forward_list<ControlListener> control_listen;

	Config();

	void Check();
};

/**
 * Load and parse the specified configuration file.  Throws an
 * exception on error.
 */
void
LoadConfigFile(Config &config, const char *path);
