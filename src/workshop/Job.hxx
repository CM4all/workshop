// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef WORKSHOP_JOB_HXX
#define WORKSHOP_JOB_HXX

#include <chrono>
#include <string>
#include <forward_list>

class WorkshopQueue;

struct WorkshopJob {
	WorkshopQueue &queue;

	std::string id, plan_name, syslog_server;

	std::forward_list<std::string> args;

	std::forward_list<std::string> env;

	explicit WorkshopJob(WorkshopQueue &_queue):queue(_queue) {}

	WorkshopJob(WorkshopQueue &_queue, const char *_id, const char *_plan_name)
		:queue(_queue), id(_id), plan_name(_plan_name) {
	}

	/**
	 * Update the "progress" value of the job.
	 *
	 * @param job the job
	 * @param progress a percent value (0 .. 100)
	 * @param timeout the timeout for the next feedback (an interval
	 * string that is understood by PostgreSQL)
	 * @return true on success
	 */
	bool SetProgress(unsigned progress, const char *timeout) noexcept;

	/**
	 * Add more environment variables to the record in the "jobs"
	 * table.  This is useful when executing the job again via
	 * SetAgain().
	 *
	 * Throws exception on error.
	 */
	void SetEnv(const char *more_env);

	/**
	 * Mark the job as "done".
	 */
	void SetDone(int status, const char *log) noexcept;

	/**
	 * Mark the job as "execute again".
	 *
	 * @param delay don't execute this job until the given duration
	 * has passed
	 */
	void SetAgain(std::chrono::seconds delay, const char *log) noexcept;

	void AddCpuUsage(std::chrono::microseconds cpu_usage) noexcept;
};

#endif
