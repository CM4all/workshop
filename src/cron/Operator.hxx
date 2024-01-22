// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Job.hxx"
#include "Result.hxx"
#include "io/Logger.hxx"
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
	const CronJob &job;

	LazyDomainLogger &logger;

	explicit CronOperator(const CronJob &_job, LazyDomainLogger &_logger) noexcept
		:job(std::move(_job)), logger(_logger) {}

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
