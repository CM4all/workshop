/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Partition.hxx"
#include "Job.hxx"
#include "../Config.hxx"
#include "EmailService.hxx"

CronPartition::CronPartition(EventLoop &event_loop,
                             SpawnService &_spawn_service,
                             CurlGlobal &_curl,
                             const Config &root_config,
                             const CronPartitionConfig &config,
                             BoundMethod<void()> _idle_callback)
    :name(config.name.empty() ? nullptr : config.name.c_str()),
     tag(config.tag.empty() ? nullptr : config.tag.c_str()),
     translation_socket(config.translation_socket.c_str()),
     logger("cron/" + config.name),
     email_service(config.qmqp_server.IsNull()
                   ? nullptr
                   : new EmailService(event_loop, config.qmqp_server)),
     queue(logger, event_loop, root_config.node_name.c_str(),
           config.database.c_str(), config.database_schema.c_str(),
           [this](CronJob &&job){ OnJob(std::move(job)); }),
     workplace(_spawn_service, email_service.get(),
               _curl, *this,
               root_config.concurrency),
     idle_callback(_idle_callback)
{
}

CronPartition::~CronPartition()
{
}

void
CronPartition::BeginShutdown()
{
    queue.Disable();
    workplace.CancelAll();

    if (email_service)
        email_service->CancelAll();
}

void
CronPartition::OnJob(CronJob &&job)
{
    logger(4, "OnJob ", job.id);

    if (!queue.Claim(job))
        return;

    try {
        workplace.Start(queue, translation_socket,
                        name, tag,
                        std::move(job));
    } catch (const std::runtime_error &e) {
        logger(1, "failed to start cronjob '", job.id, "': ",
               std::current_exception());
    }

    if (workplace.IsFull())
        queue.Disable();
}

void
CronPartition::OnChildProcessExit(int)
{
    if (!workplace.IsFull())
        queue.Enable();

    if (IsIdle())
        idle_callback();
}
