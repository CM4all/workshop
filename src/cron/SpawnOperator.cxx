/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "SpawnOperator.hxx"
#include "Workplace.hxx"
#include "Queue.hxx"
#include "spawn/Interface.hxx"
#include "event/Duration.hxx"

#include <daemon/log.h>

#include <unistd.h>
#include <sys/wait.h>

void
CronSpawnOperator::Spawn(PreparedChildProcess &&p)
try {
    pid = workplace.GetSpawnService().SpawnChildProcess(job.id.c_str(),
                                                        std::move(p), this);

    daemon_log(2, "job %s running as pid %d\n", job.id.c_str(), pid);

    /* kill after 5 minutes */
    timeout_event.Add(EventDuration<300>::value);
} catch (const std::exception &e) {
    queue.Finish(job);
    queue.InsertResult(job, start_time.c_str(), -1, e.what());
    throw;
}

void
CronSpawnOperator::Cancel()
{
    workplace.GetSpawnService().KillChildProcess(pid, SIGTERM);

    queue.Finish(job);
    queue.InsertResult(job, start_time.c_str(), -1, "Canceled");
    timeout_event.Cancel();
    workplace.OnExit(this);
}

void
CronSpawnOperator::OnChildProcessExit(int status)
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
    // TODO: capture log
    queue.InsertResult(job, start_time.c_str(), exit_status, nullptr);
    timeout_event.Cancel();
    workplace.OnExit(this);
}