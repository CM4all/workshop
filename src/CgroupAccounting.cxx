// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupAccounting.hxx"
#include "system/Error.hxx"
#include "io/UniqueFileDescriptor.hxx"

using std::string_view_literals::operator""sv;

static size_t
ReadFile(FileDescriptor fd, void *buffer, size_t buffer_size)
{
	ssize_t nbytes = pread(fd.Get(), buffer, buffer_size, 0);
	if (nbytes < 0)
		throw MakeErrno("Failed to read");

	return nbytes;
}

static char *
ReadFileZ(FileDescriptor fd, char *buffer, size_t buffer_size)
{
	size_t length = ReadFile(fd, buffer, buffer_size - 1);
	buffer[length] = 0;
	return buffer;
}

static const char *
FindLine(const char *data, const char *name)
{
	const size_t name_length = strlen(name);
	const char *p = data;

	while (true) {
		const char *needle = strstr(p, name);
		if (needle == nullptr)
			break;

		if ((needle == data || needle[-1] == '\n') && needle[name_length] == ' ')
			return needle + name_length + 1;

		p = needle + 1;
	}

	return nullptr;
}

std::chrono::microseconds
ReadCgroupCpuUsage(FileDescriptor fd)
{
	char buffer[4096];
	const char *data = ReadFileZ(fd, buffer, sizeof(buffer));

	if (const char *p = FindLine(data, "usage_usec"))
		return std::chrono::microseconds(strtoull(p, nullptr, 10));

	return std::chrono::microseconds::min();
}
