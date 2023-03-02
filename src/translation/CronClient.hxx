// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct TranslateResponse;
class AllocatorPtr;
class SocketDescriptor;

TranslateResponse
TranslateCron(AllocatorPtr alloc, SocketDescriptor s,
	      const char *partition_name, const char *listener_tag,
	      const char *user, const char *uri,
	      const char *param);
