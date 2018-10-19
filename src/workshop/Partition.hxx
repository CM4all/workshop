/*
 * Copyright 2006-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WORKSHOP_PARTITION_HXX
#define WORKSHOP_PARTITION_HXX

#include "Queue.hxx"
#include "Workplace.hxx"
#include "spawn/ExitListener.hxx"
#include "io/Logger.hxx"
#include "util/BindMethod.hxx"

struct Config;
struct WorkshopPartitionConfig;
class Instance;
class MultiLibrary;

class WorkshopPartition final : ExitListener {
	const Logger logger;

	Instance &instance;
	MultiLibrary &library;

	WorkshopQueue queue;
	WorkshopWorkplace workplace;

	BoundMethod<void()> idle_callback;

	const size_t max_log;

public:
	WorkshopPartition(Instance &instance,
			  MultiLibrary &_library,
			  SpawnService &_spawn_service,
			  const Config &root_config,
			  const WorkshopPartitionConfig &config,
			  BoundMethod<void()> _idle_callback);

	auto &GetEventLoop() const noexcept {
		return queue.GetEventLoop();
	}

	bool IsIdle() const {
		return workplace.IsEmpty();
	}

	void Start() {
		queue.Connect();
	}

	void Close() {
		queue.Close();
	}

	void BeginShutdown() {
		queue.DisableAdmin();
	}

	void DisableQueue() {
		queue.DisableAdmin();
	}

	void EnableQueue() {
		queue.EnableAdmin();
	}

	void UpdateFilter();
	void UpdateLibraryAndFilter(bool force);

private:
	bool StartJob(WorkshopJob &&job);
	void OnJob(WorkshopJob &&job);

	/* virtual methods from ExitListener */
	void OnChildProcessExit(int status) override;
};

#endif
