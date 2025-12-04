// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Job.hxx"
#include "Queue.hxx"

bool
WorkshopJob::SetProgress(unsigned progress, const char *timeout, bool notify) noexcept
{
	return queue.SetJobProgress(*this, progress, timeout, notify);
}

void
WorkshopJob::SetEnv(const char *more_env)
{
	queue.SetJobEnv(*this, more_env);
}

void
WorkshopJob::SetDone(int status, const char *log) noexcept
{
	queue.SetJobDone(*this, status, log);
}

void
WorkshopJob::SetAgain(std::chrono::seconds delay, const char *log) noexcept
{
	queue.AgainJob(*this, log, delay);
}

void
WorkshopJob::AddCpuUsage(std::chrono::microseconds cpu_usage) noexcept
{
	queue.AddJobCpuUsage(*this, cpu_usage);
}
