/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "SpawnOperator.hxx"
#include "Workplace.hxx"
#include "Queue.hxx"
#include "PipeCaptureBuffer.hxx"
#include "spawn/Interface.hxx"
#include "spawn/Prepared.hxx"
#include "event/Duration.hxx"
#include "system/Error.hxx"

#include <daemon/log.h>

#include <unistd.h>
#include <sys/wait.h>

CronSpawnOperator::CronSpawnOperator(CronQueue &_queue,
                                     CronWorkplace &_workplace, CronJob &&_job,
                                     std::string &&_start_time) noexcept
        :CronOperator(_queue, _workplace,
                      std::move(_job),
                      std::move(_start_time))
{
}

CronSpawnOperator::~CronSpawnOperator()
{
}

void
CronSpawnOperator::Spawn(PreparedChildProcess &&p)
try {
    if (p.stderr_fd < 0) {
        /* no STDERR destination configured: the default is to capture
           it and save in the cronresults table */
        UniqueFileDescriptor r, w;
        if (!UniqueFileDescriptor::CreatePipe(r, w))
            throw MakeErrno("pipe() failed");

        p.SetStderr(std::move(w));
        if (p.stdout_fd < 0)
            /* capture STDOUT as well */
            p.stdout_fd = p.stderr_fd;

        output_capture = std::make_unique<PipeCaptureBuffer>(queue.GetEventLoop(),
                                                             std::move(r));
    }

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
    output_capture.reset();
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

    const char *log = output_capture
        ? output_capture->NormalizeASCII()
        : nullptr;

    queue.Finish(job);
    queue.InsertResult(job, start_time.c_str(), exit_status, log);
    timeout_event.Cancel();
    workplace.OnExit(this);
}
