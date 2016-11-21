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
class CronQueue;
class ExitListener;

class CronWorkplace {
    SpawnService &spawn_service;
    CronQueue &queue;
    ExitListener &exit_listener;

    typedef boost::intrusive::list<Operator,
                                   boost::intrusive::constant_time_size<true>> OperatorList;

    OperatorList operators;

    const char *const translation_socket;
    const unsigned max_operators;

public:
    CronWorkplace(SpawnService &_spawn_service, CronQueue &_queue,
                  ExitListener &_exit_listener,
                  const char *_translation_socket,
                  unsigned _max_operators)
        :spawn_service(_spawn_service), queue(_queue),
         exit_listener(_exit_listener),
         translation_socket(_translation_socket),
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

    CronQueue &GetQueue() {
        return queue;
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
    void Start(CronJob &&job);

    void OnExit(Operator *o);
};

#endif
