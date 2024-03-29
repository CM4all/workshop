// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct TranslateResponse;
class AllocatorPtr;
class SocketDescriptor;

TranslateResponse
TranslateSpawn(AllocatorPtr alloc, SocketDescriptor s, const char *tag,
	       const char *plan_name, const char *execute, const char *param);
