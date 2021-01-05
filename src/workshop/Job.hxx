/*
 * Copyright 2006-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
};

#endif
