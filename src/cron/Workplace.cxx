/*
 * Manage operator processes.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Workplace.hxx"
#include "Queue.hxx"
#include "Job.hxx"
#include "SpawnOperator.hxx"
#include "CurlOperator.hxx"
#include "AllocatorPtr.hxx"
#include "translation/CronGlue.hxx"
#include "translation/Response.hxx"
#include "spawn/Prepared.hxx"
#include "spawn/Interface.hxx"
#include "system/Error.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringCompare.hxx"

#include <daemon/log.h>

#include <string>
#include <map>
#include <set>
#include <list>

#include <assert.h>

static bool
IsURL(const char *command)
{
    return StringStartsWith(command, "http://") ||
        StringStartsWith(command, "https://");
}

static std::unique_ptr<CronOperator>
MakeSpawnOperator(CronQueue &queue, CronWorkplace &workplace,
                  const char *translation_socket,
                  CronJob &&job, const char *command,
                  std::string &&start_time)
{
    const char *uri = StringStartsWith(command, "urn:")
        ? command
        : nullptr;

    /* prepare the child process */

    PreparedChildProcess p;

    if (uri == nullptr) {
        p.args.push_back("/bin/sh");
        p.args.push_back("-c");
        p.args.push_back(command);
    }

    Allocator alloc;

    try {
        const auto response = TranslateCron(alloc, translation_socket,
                                            job.account_id.c_str(),
                                            uri,
                                            job.translate_param.empty()
                                            ? nullptr
                                            : job.translate_param.c_str());

        if (uri != nullptr) {
            if (response.execute == nullptr)
                throw std::runtime_error("No EXECUTE from translation server");

            p.args.push_back(alloc.Dup(response.execute));

            for (const char *arg : response.args) {
                if (p.args.full())
                    throw std::runtime_error("Too many APPEND packets from translation server");

                p.args.push_back(alloc.Dup(arg));
            }
        }

        response.child_options.CopyTo(p);
    } catch (const std::exception &e) {
        queue.Finish(job);
        queue.InsertResult(job, start_time.c_str(), -1, e.what());
        std::throw_with_nested(FormatRuntimeError("Failed to translate job '%s'",
                                                  job.id.c_str()));
    }

    /* create operator object */

    auto o = std::make_unique<CronSpawnOperator>(queue, workplace,
                                                 workplace.GetSpawnService(),
                                                 std::move(job),
                                                 std::move(start_time));
    o->Spawn(std::move(p));
    return std::unique_ptr<CronOperator>(std::move(o));
}

static std::unique_ptr<CronOperator>
MakeCurlOperator(CronQueue &queue, CronWorkplace &workplace,
                 CurlGlobal &curl_global,
                 CronJob &&job, const char *url,
                 std::string &&start_time)
{
    auto o = std::make_unique<CronCurlOperator>(queue, workplace,
                                                std::move(job),
                                                std::move(start_time),
                                                curl_global, url);
    return std::unique_ptr<CronOperator>(std::move(o));
}

void
CronWorkplace::Start(CronQueue &queue, const char *translation_socket,
                     CronJob &&job)
{
    auto start_time = queue.GetNow();

    /* need a copy because the std::move(job) below may invalidate the
       c_str() pointer */
    const auto command = job.command;

    auto o = IsURL(command.c_str())
        ? MakeCurlOperator(queue, *this, curl,
                           std::move(job), command.c_str(),
                           std::move(start_time))
        : MakeSpawnOperator(queue, *this, translation_socket,
                            std::move(job), command.c_str(),
                            std::move(start_time));

    operators.push_back(*o.release());
}

void
CronWorkplace::OnExit(CronOperator *o)
{
    operators.erase(operators.iterator_to(*o));
    delete o;

    exit_listener.OnChildProcessExit(-1);
}
