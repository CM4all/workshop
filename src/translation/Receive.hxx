// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct TranslateResponse;
class AllocatorPtr;
class SocketDescriptor;

TranslateResponse
ReceiveTranslateResponse(AllocatorPtr alloc, SocketDescriptor s);
