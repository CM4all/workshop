// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Partition.hxx"
#include "Job.hxx"
#include "../Config.hxx"
#include "EmailService.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "net/ConnectSocket.hxx"

#ifdef HAVE_AVAHI
#include "Sticky.hxx"
#endif

using std::string_view_literals::operator""sv;

CronPartition::CronPartition(EventLoop &event_loop,
			     SpawnService &_spawn_service,
#ifdef HAVE_AVAHI
			     Avahi::Client *avahi_client,
			     Avahi::Publisher *avahi_publisher,
			     Avahi::ErrorHandler &avahi_error_handler,
#endif
			     const Config &root_config,
			     const CronPartitionConfig &config,
			     BoundMethod<void() noexcept> _idle_callback)
	:name(config.name),
	 tag(config.tag.empty() ? nullptr : config.tag.c_str()),
	 translation_socket(config.translation_socket),
	 logger(fmt::format("cron/{}"sv, config.name)),
#ifdef HAVE_AVAHI
	 sticky(config.sticky
		? new CronSticky(*avahi_client, *avahi_publisher, avahi_error_handler,
				 config.zeroconf,
				 BIND_THIS_METHOD(OnStickyChanged))
		: nullptr),
#endif
	 email_service(event_loop, config.qmqp_server),
	 pond_socket(!config.pond_server.IsNull()
	 ? CreateConnectDatagramSocket(config.pond_server)
	 : UniqueSocketDescriptor()),
	 queue(logger, event_loop, root_config.node_name.c_str(),
	       Pg::Config{config.database},
#ifdef HAVE_AVAHI
	       config.sticky,
#else
	       false,
#endif
	       [this](CronJob &&job){ OnJob(std::move(job)); }),
	 workplace(_spawn_service,
		   email_service, config.use_qrelay, config.default_email_sender,
		   pond_socket,
		   *this,
		   root_config.concurrency),
	 idle_callback(_idle_callback),
	 default_timeout(config.default_timeout)
{
}

CronPartition::~CronPartition() noexcept = default;

void
CronPartition::BeginShutdown() noexcept
{
#ifdef HAVE_AVAHI
	if (sticky)
		sticky->BeginShutdown();
#endif

	queue.DisableAdmin();
	workplace.CancelAll();
	email_service.CancelAll();
}

#ifdef HAVE_AVAHI

void
CronPartition::EnableDisableSticky() noexcept
{
	if (sticky) {
		if (queue.IsEnabledOrFull())
			sticky->Enable();
		else
			sticky->Disable();
	}
}

void
CronPartition::OnStickyChanged() noexcept
{
	queue.FlushSticky();
}

#endif // HAVE_AVAHI

void
CronPartition::OnJob(CronJob &&job) noexcept
{
	logger.Fmt(4, "OnJob {:?}"sv, job.id);

#ifdef HAVE_AVAHI
	if (!job.sticky_id.empty() && sticky) {
		if (const auto [node_name, is_local] = sticky->IsLocal(job.sticky_id);
		    !is_local) {
			queue.InsertStickyNonLocal(job.sticky_id.c_str());
			logger.Fmt(4, "Ignoring job {:?} which is sticky on node {:?} (sticky_id={:?})"sv, job.id, node_name, job.sticky_id);
			return;
		} else
			logger.Fmt(5, "Job {:?} is sticky on this node (sticky_id={:?})"sv, job.id, job.sticky_id);
	}
#endif

	if (job.timeout.count() <= 0)
		job.timeout = default_timeout;

	if (!queue.Claim(job))
		return;

	try {
		workplace.Start(queue, translation_socket,
				name, tag,
				std::move(job));
	} catch (...) {
		logger.Fmt(1, "failed to start cronjob {:?}: {}"sv,
			   job.id, std::current_exception());
	}

	if (workplace.IsFull())
		queue.DisableFull();
}

void
CronPartition::OnChildProcessExit(int) noexcept
{
	if (!workplace.IsFull())
		queue.EnableFull();

	if (IsIdle())
		idle_callback();
}
