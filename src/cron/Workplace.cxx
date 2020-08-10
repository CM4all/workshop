/*
 * Copyright 2006-2018 Content Management AG
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

#include "Workplace.hxx"
#include "Queue.hxx"
#include "Job.hxx"
#include "SpawnOperator.hxx"
#include "CurlOperator.hxx"
#include "AllocatorPtr.hxx"
#include "translation/CronGlue.hxx"
#include "translation/Response.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/Interface.hxx"
#include "system/Error.hxx"
#include "util/Exception.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringCompare.hxx"

#include <string>
#include <map>
#include <set>
#include <list>

#include <assert.h>

static bool
IsURL(const char *command)
{
	return StringStartsWith(command, "http://") ||
		StringStartsWith(command, "https://");
}

static std::unique_ptr<CronOperator>
MakeSpawnOperator(CronQueue &queue, CronWorkplace &workplace,
		  const char *translation_socket,
		  const char *partition_name, const char *listener_tag,
		  CronJob &&job, const char *command,
		  std::string &&start_time)
{
	const char *uri = StringStartsWith(command, "urn:")
		? command
		: nullptr;

	/* prepare the child process */

	PreparedChildProcess p;

	if (uri == nullptr) {
		p.args.push_back("/bin/sh");
		p.args.push_back("-c");
		p.args.push_back(command);
	}

	Allocator alloc;

	TranslateResponse response;
	try {
		response = TranslateCron(alloc, translation_socket,
					 partition_name, listener_tag,
					 job.account_id.c_str(),
					 uri,
					 job.translate_param.empty()
					 ? nullptr
					 : job.translate_param.c_str());

		if (response.status != 0) {
			if (response.message != nullptr)
				throw FormatRuntimeError("Status %u from translation server: %s",
							 response.status,
							 response.message);

			throw FormatRuntimeError("Status %u from translation server",
						 response.status);
		}

		if (response.child_options.uid_gid.IsEmpty())
			throw std::runtime_error("No UID_GID from translation server");

		if (uri != nullptr) {
			if (response.execute == nullptr)
				throw std::runtime_error("No EXECUTE from translation server");

			p.args.push_back(alloc.Dup(response.execute));

			for (const char *arg : response.args) {
				if (p.args.size() >= 4096)
					throw std::runtime_error("Too many APPEND packets from translation server");

				p.args.push_back(alloc.Dup(arg));
			}
		}

		response.child_options.CopyTo(p);
	} catch (...) {
		queue.Finish(job);
		queue.InsertResult(job, start_time.c_str(), -1, GetFullMessage(std::current_exception()).c_str());
		std::throw_with_nested(std::runtime_error("Translation failed"));
	}

	/* create operator object */

	auto o = std::make_unique<CronSpawnOperator>(queue, workplace,
						     workplace.GetSpawnService(),
						     std::move(job),
						     std::move(start_time));
	o->Spawn(std::move(p), workplace.GetPondSocket());
	return std::unique_ptr<CronOperator>(std::move(o));
}

static std::unique_ptr<CronOperator>
MakeCurlOperator(CronQueue &queue, CronWorkplace &workplace,
		 CurlGlobal &curl_global,
		 CronJob &&job, const char *url,
		 std::string &&start_time)
{
	auto o = std::make_unique<CronCurlOperator>(queue, workplace,
						    std::move(job),
						    std::move(start_time),
						    curl_global, url);
	o->Start();
	return std::unique_ptr<CronOperator>(std::move(o));
}

void
CronWorkplace::Start(CronQueue &queue, const char *translation_socket,
		     const char *partition_name, const char *listener_tag,
		     CronJob &&job)
{
	auto start_time = queue.GetNow();

	/* need a copy because the std::move(job) below may invalidate the
	   c_str() pointer */
	const auto command = job.command;

	auto o = IsURL(command.c_str())
		? MakeCurlOperator(queue, *this, curl,
				   std::move(job), command.c_str(),
				   std::move(start_time))
		: MakeSpawnOperator(queue, *this, translation_socket,
				    partition_name, listener_tag,
				    std::move(job), command.c_str(),
				    std::move(start_time));

	operators.push_back(*o.release());
}

void
CronWorkplace::OnExit(CronOperator *o)
{
	operators.erase(operators.iterator_to(*o));
	delete o;

	exit_listener.OnChildProcessExit(-1);
}
