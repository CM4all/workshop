// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/AllocatedString.hxx"

#include <exception>

class CronHandler {
public:
	virtual void OnFinish(const CronResult &result) noexcept = 0;
	virtual void OnExit() noexcept = 0;
};
