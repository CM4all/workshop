/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_WORKPLACE_HXX
#define WORKSHOP_WORKPLACE_HXX

#include "Operator.hxx"

#include <inline/compiler.h>

#include <boost/intrusive/list.hpp>

#include <memory>
#include <string>

#include <assert.h>

struct Plan;
struct Job;
class Instance;
class SpawnService;

class Workplace {
    Instance &instance;
    SpawnService &spawn_service;

    const std::string node_name;

    typedef boost::intrusive::list<Operator,
                                   boost::intrusive::constant_time_size<true>> OperatorList;

    OperatorList operators;

    const unsigned max_operators;

public:
    Workplace(Instance &_instance, SpawnService &_spawn_service,
              const char *_node_name,
              unsigned _max_operators)
        :instance(_instance), spawn_service(_spawn_service),
         node_name(_node_name),
         max_operators(_max_operators) {
        assert(max_operators > 0);
    }

    Workplace(const Workplace &other) = delete;

    ~Workplace() {
        assert(operators.empty());
    }

    gcc_pure
    const char *GetNodeName() const {
        return node_name.c_str();
    }

    bool IsEmpty() const {
        return operators.empty();
    }

    bool IsFull() const {
        return operators.size() == max_operators;
    }

    gcc_pure
    std::string GetRunningPlanNames() const;

    /**
     * Returns the plan names which have reached their concurrency
     * limit.
     */
    gcc_pure
    std::string GetFullPlanNames() const;

    /**
     * Throws std::runtime_error on error.
     */
    void Start(EventLoop &event_loop, const Job &job,
               std::shared_ptr<Plan> &&plan);

    void OnExit(Operator *o);
};

#endif
