// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "RateLimit.hxx"
#include "pg/Interval.hxx"
#include "util/StringStrip.hxx"

#include <stdexcept>

#include <stdlib.h>

RateLimit
RateLimit::Parse(const char *s)
{
	s = StripLeft(s);

	char *endptr;
	const unsigned max_count = strtoul(s, &endptr, 10);
	if (endptr == s)
		throw std::runtime_error("Failed to parse number");

	s = StripLeft(endptr);
	if (*s != '/')
		throw std::runtime_error("'/' expected");

	++s;

	const auto duration = Pg::ParseIntervalS(StripLeft(s));
	return {duration, max_count};
}
