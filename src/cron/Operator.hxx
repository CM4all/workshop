// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Job.hxx"
#include "io/Logger.hxx"

#include <string>

class ChildProcessRegistry;
class CronHandler;
struct CronResult;

/**
 * A #CronJob being executed.
 */
class CronOperator
	: LoggerDomainFactory
{
	CronHandler &handler;

protected:
	const CronJob job;

	LazyDomainLogger logger;

private:
	const std::string tag;

public:
	CronOperator(CronHandler &_handler, CronJob &&_job,
		     std::string_view _tag) noexcept;

	virtual ~CronOperator() noexcept = default;

	CronOperator(const CronOperator &other) = delete;
	CronOperator &operator=(const CronOperator &other) = delete;

	bool IsTag(std::string_view _tag) const noexcept {
		return tag == _tag;
	}

protected:
	void Finish(const CronResult &result) noexcept;

private:
	void OnTimeout() noexcept;

	/* virtual methods from LoggerDomainFactory */
	std::string MakeLoggerDomain() const noexcept;
};
