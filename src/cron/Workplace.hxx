/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_WORKPLACE_HXX
#define CRON_WORKPLACE_HXX

#include "Operator.hxx"

#include <inline/compiler.h>

#include <boost/intrusive/list.hpp>

#include <assert.h>

struct CronJob;
class SpawnService;
class CurlGlobal;
class CronQueue;
class ExitListener;

class CronWorkplace {
    SpawnService &spawn_service;
    CurlGlobal &curl;
    ExitListener &exit_listener;

    typedef boost::intrusive::list<CronOperator,
                                   boost::intrusive::constant_time_size<true>> OperatorList;

    OperatorList operators;

    const unsigned max_operators;

public:
    CronWorkplace(SpawnService &_spawn_service,
                  CurlGlobal &_curl,
                  ExitListener &_exit_listener,
                  unsigned _max_operators)
        :spawn_service(_spawn_service),
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
               CronJob &&job);

    void OnExit(CronOperator *o);
};

#endif
