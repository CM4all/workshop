// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

namespace Co { template<typename T> class Task; }
struct TranslateResponse;
class AllocatorPtr;
class EventLoop;
class SocketDescriptor;

Co::Task<TranslateResponse>
TranslateSpawn(EventLoop &event_loop,
	       AllocatorPtr alloc, SocketDescriptor s,
	       const char *tag,
	       const char *plan_name, const char *execute, const char *param);
