/*
 * Manage operator processes.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Workplace.hxx"
#include "Queue.hxx"
#include "Job.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/Interface.hxx"
#include "system/Error.hxx"
#include "util/RuntimeError.hxx"

#include <daemon/log.h>

#include <string>
#include <map>
#include <set>
#include <list>

#include <assert.h>

void
CronWorkplace::Start(CronJob &&job)
{
    /* prepare the child process */

    PreparedChildProcess p;
    p.args.push_back("/bin/sh");
    p.args.push_back("-c");
    p.args.push_back(job.command.c_str());

    // TODO: query translation server

    /* create operator object */

    auto o = std::make_unique<Operator>(*this, std::move(job));
    o->Spawn(std::move(p));

    operators.push_back(*o.release());
}

void
CronWorkplace::OnExit(Operator *o)
{
    operators.erase(operators.iterator_to(*o));
    delete o;

    exit_listener.OnChildProcessExit(-1);
}
