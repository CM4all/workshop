// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupAccounting.hxx"
#include "io/SmallTextFile.hxx"
#include "util/NumberParser.hxx"
#include "util/StringCompare.hxx"

using std::string_view_literals::operator""sv;

std::chrono::microseconds
ReadCgroupCpuUsage(FileDescriptor fd)
{
	for (std::string_view line : IterableSmallTextFile<4096>(fd)) {
		if (SkipPrefix(line, "usage_usec "sv)) {
			const auto usec = ParseInteger<uint_least64_t>(line);
			if (!usec)
				break;

			return std::chrono::microseconds(*usec);
		}
	}

	return std::chrono::microseconds::min();
}
