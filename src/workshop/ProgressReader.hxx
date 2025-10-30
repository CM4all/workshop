// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/PipeEvent.hxx"
#include "util/BindMethod.hxx"
#include "util/StaticVector.hxx"

class UniqueFileDescriptor;

class ProgressReader {
	PipeEvent event;
	StaticVector<char, 64> stdout_buffer;
	unsigned last_progress = 0;

	typedef BoundMethod<void(unsigned value) noexcept> Callback;
	const Callback callback;

public:
	ProgressReader(EventLoop &event_loop,
		       UniqueFileDescriptor _fd,
		       Callback _callback) noexcept;

	~ProgressReader() noexcept {
		event.Close();
	}

private:
	void PipeReady(unsigned) noexcept;
};
