// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Result.hxx"
#include "co/AwaitableHelper.hxx"

/**
 * A #CronJob being executed.
 */
class CronOperator
{
	std::coroutine_handle<> continuation;

	CronResult value;

	bool ready = false;

	using Awaitable = Co::AwaitableHelper<CronOperator, false>;
	friend Awaitable;

protected:
	CronOperator() noexcept = default;

public:
	virtual ~CronOperator() noexcept = default;

	CronOperator(const CronOperator &other) = delete;
	CronOperator &operator=(const CronOperator &other) = delete;

	Awaitable operator co_await() noexcept {
		return *this;
	}

protected:
	void Finish(CronResult &&result) noexcept {
		value = std::move(result);
		ready = true;

		if (continuation)
			continuation.resume();
	}

private:
	bool IsReady() const noexcept {
		return ready;
	}

	CronResult TakeValue() noexcept {
		return std::move(value);
	}
};
