// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CaptureBuffer.hxx"
#include "util/AllocatedString.hxx"
#include "util/CharUtil.hxx"

#include <algorithm>

static constexpr bool
IsAllowedNonPrintableChar(char ch) noexcept
{
	return ch == '\r' || ch == '\n' || ch == '\t';
}

static constexpr bool
IsDisallowedChar(char ch) noexcept
{
	return !IsPrintableASCII(ch) && !IsAllowedNonPrintableChar(ch);
}

AllocatedString
CaptureBuffer::NormalizeASCII() && noexcept
{
	if (size == capacity)
		/* crop the last character to make room for the null
		   terminator */
		size = capacity - 1;

	std::replace_if(data.get(), std::next(data.get(), size),
			IsDisallowedChar, ' ');
	data[size] = 0;
	return AllocatedString::Donate(data.release());
}
