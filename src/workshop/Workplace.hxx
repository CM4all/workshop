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
struct WorkshopJob;
class SpawnService;
class ExitListener;

class WorkshopWorkplace {
    SpawnService &spawn_service;
    ExitListener &exit_listener;

    const std::string node_name;

    typedef boost::intrusive::list<WorkshopOperator,
                                   boost::intrusive::constant_time_size<true>> OperatorList;

    OperatorList operators;

    const unsigned max_operators;

public:
    WorkshopWorkplace(SpawnService &_spawn_service,
                      ExitListener &_exit_listener,
                      const char *_node_name,
                      unsigned _max_operators)
        :spawn_service(_spawn_service), exit_listener(_exit_listener),
         node_name(_node_name),
         max_operators(_max_operators) {
        assert(max_operators > 0);
    }

    WorkshopWorkplace(const WorkshopWorkplace &other) = delete;

    ~WorkshopWorkplace() {
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
    void Start(EventLoop &event_loop, const WorkshopJob &job,
               std::shared_ptr<Plan> &&plan);

    void OnExit(WorkshopOperator *o);
};

#endif
