// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Workplace.hxx"
#include "Queue.hxx"
#include "Job.hxx"
#include "SpawnOperator.hxx"
#include "CurlOperator.hxx"
#include "AllocatorPtr.hxx"
#include "translation/CronGlue.hxx"
#include "translation/Response.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/Interface.hxx"
#include "system/Error.hxx"
#include "net/SocketAddress.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/Exception.hxx"
#include "util/StringCompare.hxx"

#include <cassert>
#include <string>

CronWorkplace::CronWorkplace(SpawnService &_spawn_service,
			     EmailService *_email_service,
			     SocketDescriptor _pond_socket,
			     CurlGlobal &_curl,
			     ExitListener &_exit_listener,
			     std::size_t _max_operators)
	:spawn_service(_spawn_service),
	 email_service(_email_service),
	 pond_socket(_pond_socket),
	 curl(_curl),
	 exit_listener(_exit_listener),
	 max_operators(_max_operators)
{
	assert(max_operators > 0);
}

CronWorkplace::~CronWorkplace() noexcept
{
	assert(operators.empty());
}

static bool
IsURL(const char *command)
{
	return StringStartsWith(command, "http://") ||
		StringStartsWith(command, "https://");
}

static std::unique_ptr<CronOperator>
MakeSpawnOperator(CronQueue &queue, CronWorkplace &workplace,
		  SocketAddress translation_socket,
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
		try {
			response = TranslateCron(alloc, translation_socket,
						 partition_name, listener_tag,
						 job.account_id.c_str(),
						 uri,
						 job.translate_param.empty()
						 ? nullptr
						 : job.translate_param.c_str());
		} catch (...) {
			std::throw_with_nested(std::runtime_error("Translation failed"));
		}

		if (response.status != HttpStatus{}) {
			if (response.message != nullptr)
				throw FmtRuntimeError("Status {} from translation server: {}",
						      static_cast<unsigned>(response.status),
						      response.message);

			throw FmtRuntimeError("Status {} from translation server",
					      static_cast<unsigned>(response.status));
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
		throw;
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
CronWorkplace::Start(CronQueue &queue, SocketAddress translation_socket,
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
	operators.erase_and_dispose(operators.iterator_to(*o),
				    DeleteDisposer{});

	exit_listener.OnChildProcessExit(-1);
}

void
CronWorkplace::CancelAll() noexcept
{
	while (!operators.empty())
		operators.front().Cancel();
}
