// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <forward_list>
#include <string>

namespace Co { template<typename T> class Task; }
struct TranslateResponse;
class AllocatorPtr;
class EventLoop;
class SocketDescriptor;

Co::Task<TranslateResponse>
TranslateSpawn(EventLoop &event_loop,
	       AllocatorPtr alloc, SocketDescriptor s,
	       const char *tag,
	       const char *plan_name, const char *execute, const char *param,
	       const std::forward_list<std::string> &args);
