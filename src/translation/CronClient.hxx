// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string_view>

namespace Co { template<typename T> class Task; }
struct TranslateResponse;
class AllocatorPtr;
class EventLoop;
class SocketDescriptor;

Co::Task<TranslateResponse>
TranslateCron(EventLoop &event_loop,
	      AllocatorPtr alloc, SocketDescriptor s,
	      std::string_view partition_name,
	      const char *listener_tag,
	      const char *user, const char *uri,
	      const char *param);
