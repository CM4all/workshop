// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef WORKSHOP_DEBUG_H
#define WORKSHOP_DEBUG_H

#include <stdbool.h>

#ifdef NDEBUG
static const bool debug_mode = false;
#else
extern bool debug_mode;
#endif

#endif
