// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <sys/stat.h>

constexpr bool
operator==(const struct statx_timestamp &a, const struct statx_timestamp &b) noexcept
{
	return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec;
}
