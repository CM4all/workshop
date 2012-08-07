/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "Library.hxx"
#include "Queue.hxx"
#include "Workplace.hxx"

#include <daemon/log.h>

#include <signal.h>

Instance::Instance()
    :sigterm_event(SIGTERM, [this](){ OnExit(); }),
     sigint_event(SIGINT, [this](){ OnExit(); }),
     sigquit_event(SIGQUIT, [this](){ OnExit(); }),
     sighup_event(SIGHUP, [this](){ OnReload(); }),
     sigchld_event(SIGCHLD, [this](){ OnChild(); })
{
}

void
Instance::UpdateFilter()
{
    queue->SetFilter(library->GetPlanNames(),
                     workplace->GetFullPlanNames(),
                     workplace->GetRunningPlanNames());
}

void
Instance::UpdateLibraryAndFilter()
{
    library->Update();
    UpdateFilter();
}

void
Instance::OnExit()
{
    if (should_exit)
        return;

    should_exit = true;

    sigterm_event.Delete();
    sigint_event.Delete();
    sigquit_event.Delete();
    sighup_event.Delete();
    sigchld_event.Delete();

    queue->Disable();

    if (workplace != NULL) {
        if (workplace->IsEmpty()) {
            delete workplace;
            workplace = NULL;

            if (queue != NULL) {
                delete queue;
                queue = NULL;
            }
        } else {
            daemon_log(1, "waiting for operators to finish\n");
        }
    }
}

void
Instance::OnReload()
{
    if (queue == NULL)
        return;

    daemon_log(4, "reloading\n");
    UpdateLibraryAndFilter();
    queue->Reschedule();
}

void
Instance::OnChild()
{
    if (workplace == NULL)
        return;

    workplace->WaitPid();

    if (should_exit) {
        if (workplace->IsEmpty()) {
            delete workplace;
            workplace = NULL;

            if (queue != NULL) {
                delete queue;
                queue = NULL;
            }
        }
    } else {
        UpdateLibraryAndFilter();

        if (!workplace->IsFull())
            queue->Enable();
    }
}

