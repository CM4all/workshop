/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Partition.hxx"
#include "Instance.hxx"
#include "Job.hxx"
#include "util/PrintException.hxx"

CronPartition::CronPartition(CronInstance &_instance,
                             const CronConfig &root_config,
                             const CronConfig::Partition &config)
    :instance(_instance),
     translation_socket(config.translation_socket.c_str()),
     queue(instance.GetEventLoop(), root_config.node_name.c_str(),
           config.database.c_str(), "",
           [this](CronJob &&job){ OnJob(std::move(job)); })
{
}

void
CronPartition::OnJob(CronJob &&job)
{
    printf("OnJob '%s'\n", job.id.c_str());

    if (!queue.Claim(job))
        return;

    auto &workplace = instance.GetWorkplace();

    try {
        workplace.Start(queue, translation_socket,
                        std::move(job));
    } catch (const std::runtime_error &e) {
        PrintException(e);
    }

    if (workplace.IsFull())
        instance.DisableAllQueues();
}
