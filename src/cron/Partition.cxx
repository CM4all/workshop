/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Partition.hxx"
#include "Job.hxx"
#include "../Config.hxx"
#include "util/PrintException.hxx"

CronPartition::CronPartition(EventLoop &event_loop,
                             SpawnService &_spawn_service,
                             CurlGlobal &_curl,
                             const Config &root_config,
                             const CronPartitionConfig &config,
                             BoundMethod<void()> _idle_callback)
    :translation_socket(config.translation_socket.c_str()),
     queue(event_loop, root_config.node_name.c_str(),
           config.database.c_str(), config.database_schema.c_str(),
           [this](CronJob &&job){ OnJob(std::move(job)); }),
     workplace(_spawn_service, _curl, *this,
               root_config.concurrency),
     idle_callback(_idle_callback)
{
}

void
CronPartition::BeginShutdown()
{
    queue.Disable();
    workplace.CancelAll();
}

void
CronPartition::OnJob(CronJob &&job)
{
    printf("OnJob '%s'\n", job.id.c_str());

    if (!queue.Claim(job))
        return;

    try {
        workplace.Start(queue, translation_socket,
                        std::move(job));
    } catch (const std::runtime_error &e) {
        PrintException(e);
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
