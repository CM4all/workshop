/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_WORKPLACE_HXX
#define CRON_WORKPLACE_HXX

#include "Operator.hxx"

#include "util/Compiler.h"

#include <boost/intrusive/list.hpp>

#include <assert.h>

struct CronJob;
class SpawnService;
class EmailService;
class CurlGlobal;
class CronQueue;
class ExitListener;

class CronWorkplace {
    SpawnService &spawn_service;
    EmailService *const email_service;

    CurlGlobal &curl;
    ExitListener &exit_listener;

    typedef boost::intrusive::list<CronOperator,
                                   boost::intrusive::constant_time_size<true>> OperatorList;

    OperatorList operators;

    const unsigned max_operators;

public:
    CronWorkplace(SpawnService &_spawn_service,
                  EmailService *_email_service,
                  CurlGlobal &_curl,
                  ExitListener &_exit_listener,
                  unsigned _max_operators)
        :spawn_service(_spawn_service),
         email_service(_email_service),
         curl(_curl),
         exit_listener(_exit_listener),
         max_operators(_max_operators) {
        assert(max_operators > 0);
    }

    CronWorkplace(const CronWorkplace &other) = delete;

    ~CronWorkplace() {
        assert(operators.empty());
    }

    SpawnService &GetSpawnService() {
        return spawn_service;
    }

    EmailService *GetEmailService() {
        return email_service;
    }

    bool IsEmpty() const {
        return operators.empty();
    }

    bool IsFull() const {
        return operators.size() == max_operators;
    }

    /**
     * Throws std::runtime_error on error.
     */
    void Start(CronQueue &queue, const char *translation_socket,
               const char *partition_name, const char *listener_tag,
               CronJob &&job);

    void OnExit(CronOperator *o);

    void CancelAll() {
        while (!operators.empty())
            operators.front().Cancel();
    }
};

#endif
