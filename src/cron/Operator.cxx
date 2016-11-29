/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Operator.hxx"
#include "Workplace.hxx"
#include "Queue.hxx"
#include "spawn/Interface.hxx"

#include <daemon/log.h>

#include <unistd.h>
#include <sys/wait.h>

Operator::Operator(CronQueue &_queue, CronWorkplace &_workplace,
                   CronJob &&_job)
    :queue(_queue), workplace(_workplace), job(std::move(_job))
{
}

void
Operator::Spawn(PreparedChildProcess &&p)
try {
    pid = workplace.GetSpawnService().SpawnChildProcess(job.id.c_str(),
                                                        std::move(p), this);

    daemon_log(2, "job %s running as pid %d\n", job.id.c_str(), pid);
} catch (...) {
    queue.Finish(job);
    throw;
}

void
Operator::OnChildProcessExit(int status)
{
    int exit_status = WEXITSTATUS(status);

    if (WIFSIGNALED(status)) {
        daemon_log(1, "job %s (pid %d) died from signal %d%s\n",
                   job.id.c_str(), pid,
                   WTERMSIG(status),
                   WCOREDUMP(status) ? " (core dumped)" : "");
        exit_status = -1;
    } else if (exit_status == 0)
        daemon_log(3, "job %s (pid %d) exited with success\n",
                   job.id.c_str(), pid);
    else
        daemon_log(2, "job %s (pid %d) exited with status %d\n",
                   job.id.c_str(), pid,
                   exit_status);

    queue.Finish(job);
    workplace.OnExit(this);
}
