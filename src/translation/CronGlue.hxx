// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct TranslateResponse;
class SocketAddress;
class AllocatorPtr;

TranslateResponse
TranslateCron(AllocatorPtr alloc, SocketAddress socket_path,
	      const char *partition_name, const char *listener_tag,
	      const char *user, const char *uri, const char *param);
