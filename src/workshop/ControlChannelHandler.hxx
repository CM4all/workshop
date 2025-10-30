// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <chrono>
#include <exception>

class UniqueFileDescriptor;
namespace Co { template<typename t> class Task; }

class WorkshopControlChannelHandler {
public:
	virtual void OnControlProgress(unsigned progress) noexcept = 0;
	virtual void OnControlSetEnv(const char *s) noexcept = 0;
	virtual void OnControlAgain(std::chrono::seconds d) noexcept = 0;

	/**
	 * Throws on error.
	 *
	 * @return a pidfd
	 */
	virtual Co::Task<UniqueFileDescriptor> OnControlSpawn(const char *token,
							      const char *param) = 0;

	virtual void OnControlTemporaryError(std::exception_ptr &&error) noexcept = 0;
	virtual void OnControlPermanentError(std::exception_ptr &&error) noexcept = 0;
	virtual void OnControlClosed() noexcept = 0;
};
