// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <chrono>

class FileDescriptor;

std::chrono::microseconds
ReadCgroupCpuUsage(FileDescriptor fd);
