// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Job.hxx"
#include "io/Logger.hxx"

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

public:
	CronOperator(CronHandler &_handler, CronJob &&_job) noexcept;

	virtual ~CronOperator() noexcept = default;

	CronOperator(const CronOperator &other) = delete;
	CronOperator &operator=(const CronOperator &other) = delete;

protected:
	void Finish(const CronResult &result) noexcept;

private:
	/* virtual methods from LoggerDomainFactory */
	std::string MakeLoggerDomain() const noexcept;
};
