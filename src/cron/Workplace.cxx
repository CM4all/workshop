// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Workplace.hxx"
#include "Queue.hxx"
#include "Job.hxx"
#include "Result.hxx"
#include "Notification.hxx"
#include "SpawnOperator.hxx"
#include "CurlOperator.hxx"
#include "AllocatorPtr.hxx"
#include "translation/CronGlue.hxx"
#include "translation/Response.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/Interface.hxx"
#include "event/FarTimerEvent.hxx"
#include "system/Error.hxx"
#include "net/SocketAddress.hxx"
#include "co/InvokeTask.hxx"
#include "co/Task.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/StringCompare.hxx"
#include "debug.h"

#include <cassert>
#include <string>

using std::string_view_literals::operator""sv;

class CronWorkplace::Running final
	: public IntrusiveListHook<>, LoggerDomainFactory
{
	CronQueue &queue;
	CronWorkplace &workplace;
	const CronJob job;
	const std::string start_time;

	FarTimerEvent timeout_event;

	Co::InvokeTask task;

	std::string site, tag;

	LazyDomainLogger logger{*this};

public:
	Running(CronQueue &_queue, CronWorkplace &_workplace,
		CronJob &&_job, std::string &&_start_time) noexcept
		:queue(_queue), workplace(_workplace),
		 job(std::move(_job)),
		 start_time(std::move(_start_time)),
		 timeout_event(queue.GetEventLoop(),
			       BIND_THIS_METHOD(OnTimeout)),
		 site(job.account_id) {}

	auto &GetEventLoop() const noexcept {
		return timeout_event.GetEventLoop();
	}

	bool IsTag(std::string_view _tag) const noexcept {
		return tag == _tag;
	}

	void Start(SocketAddress translation_socket,
		   std::string_view partition_name,
		   const char *listener_tag) noexcept {
		/* kill after the timeout expires */
		if (job.timeout.count() > 0)
			timeout_event.Schedule(job.timeout);

		task = CoStart(translation_socket, partition_name, listener_tag);
		task.Start(BIND_THIS_METHOD(OnCompletion));
	}

	void Cancel() noexcept {
		SetResult(CronResult::Error("Canceled"sv));
	}

private:
	Co::Task<std::unique_ptr<CronOperator>> MakeOperator(SocketAddress translation_socket,
							     std::string_view partition_name,
							     const char *listener_tag);

	Co::InvokeTask CoStart(SocketAddress translation_socket,
			       std::string_view partition_name,
			       const char *listener_tag);

	void OnCompletion(std::exception_ptr error) noexcept {
		if (error) {
			logger(1, error);
			SetResult(CronResult::Error(error));
		}

		workplace.OnCompletion(*this);
	}

	void OnTimeout() noexcept {
		logger(1, "Timeout");
		SetResult(CronResult::Error("Timeout"sv));
		workplace.OnCompletion(*this);
	}

	void SetResult(const CronResult &result) noexcept;

	/* virtual methods from LoggerDomainFactory */
	std::string MakeLoggerDomain() const noexcept {
		return fmt::format("cron job={} account={}", job.id, site);
	}
};

void
CronWorkplace::Running::SetResult(const CronResult &result) noexcept
{
	if (!job.notification.empty()) {
		auto *es = workplace.GetEmailService();
		if (es != nullptr)
			SendNotificationEmail(*es, job, result);
	}

	queue.Finish(job);
	queue.InsertResult(job, start_time.c_str(), result);
}

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
	assert(running.empty());
}

[[gnu::pure]]
static bool
IsURL(std::string_view command) noexcept
{
	return command.starts_with("http://"sv) ||
		command.starts_with("https://"sv);
}

static std::unique_ptr<CronOperator>
MakeSpawnOperator(EventLoop &event_loop, SpawnService &spawn_service,
		  SocketDescriptor pond_socket,
		  LazyDomainLogger &logger,
		  const CronJob &job, const char *command,
		  const TranslateResponse &response)
{
	/* prepare the child process */

	PreparedChildProcess p;

	if (command != nullptr) {
		p.args.push_back("/bin/sh");
		p.args.push_back("-c");
		p.args.push_back(command);
	}

	if (response.status != HttpStatus{}) {
		if (response.message != nullptr)
			throw FmtRuntimeError("Status {} from translation server: {}",
					      static_cast<unsigned>(response.status),
					      response.message);

		throw FmtRuntimeError("Status {} from translation server",
				      static_cast<unsigned>(response.status));
	}

	if (response.child_options.uid_gid.IsEmpty() && !debug_mode)
		throw std::runtime_error("No UID_GID from translation server");

	if (command == nullptr) {
		if (response.execute == nullptr)
			throw std::runtime_error("No EXECUTE from translation server");

		p.args.push_back(response.execute);

		for (const char *arg : response.args) {
			if (p.args.size() >= 4096)
				throw std::runtime_error("Too many APPEND packets from translation server");

			p.args.push_back(arg);
		}
	}

	response.child_options.CopyTo(p);

	/* TODO
	if (response.timeout.count() > 0)
		job.timeout = response.timeout;
	*/

	/* create operator object */

	std::string_view site = job.account_id;
	if (response.site != nullptr)
		site = response.site;

	auto o = std::make_unique<CronSpawnOperator>(logger);
	o->Spawn(event_loop, spawn_service,
		 job.id.c_str(), site,
		 std::move(p), pond_socket);
	return std::unique_ptr<CronOperator>(std::move(o));
}

static std::unique_ptr<CronOperator>
MakeCurlOperator(CurlGlobal &curl_global,
		 LazyDomainLogger &logger,
		 const char *url)
{
	auto o = std::make_unique<CronCurlOperator>(logger,
						    curl_global, url);
	o->Start();
	return std::unique_ptr<CronOperator>(std::move(o));
}

inline Co::Task<std::unique_ptr<CronOperator>>
CronWorkplace::Running::MakeOperator(SocketAddress translation_socket,
				     std::string_view partition_name,
				     const char *listener_tag)
{
	const char *const uri = job.command.starts_with("urn:"sv)
		? job.command.c_str()
		: nullptr;

	Allocator alloc;

	TranslateResponse response;
	try {
		response = co_await
			TranslateCron(GetEventLoop(),
				      alloc, translation_socket,
				      partition_name, listener_tag,
				      job.account_id.c_str(),
				      uri,
				      job.translate_param.empty()
				      ? nullptr
				      : job.translate_param.c_str());
	} catch (...) {
		std::throw_with_nested(std::runtime_error("Translation failed"));
	}

	if (response.site != nullptr)
		site = response.site;

	if (!response.child_options.tag.empty())
		tag = response.child_options.tag;

	if (IsURL(job.command))
		co_return MakeCurlOperator(workplace.curl, logger,
					   job.command.c_str());
	else
		co_return MakeSpawnOperator(GetEventLoop(),
					    workplace.GetSpawnService(),
					    workplace.GetPondSocket(),
					    logger,
					    job,
					    uri == nullptr ? job.command.c_str() : nullptr,
					    response);
}

inline Co::InvokeTask
CronWorkplace::Running::CoStart(SocketAddress translation_socket,
				std::string_view partition_name,
				const char *listener_tag)
{
	auto op = co_await MakeOperator(translation_socket,
					partition_name, listener_tag);
	assert(op);

	const auto result = co_await *op;
	SetResult(result);
}

void
CronWorkplace::Start(CronQueue &queue, SocketAddress translation_socket,
		     std::string_view partition_name, const char *listener_tag,
		     CronJob &&job)
{
	auto *r = new Running(queue, *this,
			      std::move(job), queue.GetNow());
	running.push_back(*r);

	r->Start(translation_socket,
		 partition_name, listener_tag);
}

inline void
CronWorkplace::OnCompletion(Running &r) noexcept
{
	running.erase_and_dispose(running.iterator_to(r),
				  DeleteDisposer{});

	exit_listener.OnChildProcessExit(-1);
}

void
CronWorkplace::CancelAll() noexcept
{
	if (running.empty())
		return;

	running.clear_and_dispose([](auto *r){
		r->Cancel();
		delete r;
	});

	exit_listener.OnChildProcessExit(-1);
}

void
CronWorkplace::CancelTag(std::string_view tag) noexcept
{
	const auto n = running.remove_and_dispose_if([tag](const auto &r){
		return r.IsTag(tag);
	}, [](auto *r) {
		r->Cancel();
		delete r;
	});

	if (n > 0)
		exit_listener.OnChildProcessExit(-1);
}
