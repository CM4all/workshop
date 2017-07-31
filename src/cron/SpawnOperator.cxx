/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "SpawnOperator.hxx"
#include "Workplace.hxx"
#include "PipeCaptureBuffer.hxx"
#include "spawn/Interface.hxx"
#include "spawn/Prepared.hxx"
#include "event/Duration.hxx"
#include "system/Error.hxx"
#include "util/Exception.hxx"

#include <unistd.h>
#include <sys/wait.h>

CronSpawnOperator::CronSpawnOperator(CronQueue &_queue,
                                     CronWorkplace &_workplace,
                                     SpawnService &_spawn_service,
                                     CronJob &&_job,
                                     std::string &&_start_time) noexcept
        :CronOperator(_queue, _workplace,
                      std::move(_job),
                      std::move(_start_time)),
         spawn_service(_spawn_service)
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

        output_capture = std::make_unique<PipeCaptureBuffer>(GetEventLoop(),
                                                             std::move(r),
                                                             8192);
    }

    /* change to home directory (if one was set) */
    if (p.ns.home != nullptr)
        p.chdir = p.ns.mount_home != nullptr
            ? p.ns.mount_home
            : p.ns.home;

    pid = spawn_service.SpawnChildProcess(job.id.c_str(), std::move(p), this);

    logger(2, "running");

    /* kill after 5 minutes */
    timeout_event.Add(EventDuration<300>::value);
} catch (const std::exception &e) {
    Finish(-1, GetFullMessage(e).c_str());
    throw;
}

void
CronSpawnOperator::Cancel()
{
    output_capture.reset();
    spawn_service.KillChildProcess(pid, SIGTERM);

    Finish(-1, "Canceled");
    timeout_event.Cancel();
    workplace.OnExit(this);
}

void
CronSpawnOperator::OnChildProcessExit(int status)
{
    int exit_status = WEXITSTATUS(status);

    if (WIFSIGNALED(status)) {
        logger(1, "died from signal ",
               WTERMSIG(status),
               WCOREDUMP(status) ? " (core dumped)" : "");
        exit_status = -1;
    } else if (exit_status == 0)
        logger(3, "exited with success");
    else
        logger(2, "exited with status ", exit_status);

    const char *log = output_capture
        ? output_capture->NormalizeASCII()
        : nullptr;

    Finish(exit_status, log);
    timeout_event.Cancel();
    workplace.OnExit(this);
}
