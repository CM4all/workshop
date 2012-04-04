/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_WORKPLACE_H
#define WORKSHOP_WORKPLACE_H

#include "operator.hxx"

#include <inline/compiler.h>

#include <string>
#include <list>

#include <assert.h>

struct Plan;
struct Job;
struct Operator;

struct Workplace {
    std::string node_name;

    typedef std::list<Operator *> OperatorList;
    OperatorList operators;

    unsigned max_operators, num_operators;

    std::string plan_names, full_plan_names;

    Workplace(const char *_node_name, unsigned _max_operators)
        :node_name(_node_name),
         max_operators(_max_operators), num_operators(0) {
        assert(max_operators > 0);
    }

    Workplace(const Workplace &other) = delete;

    ~Workplace() {
        assert(operators.empty());
        assert(num_operators == 0);
    }

    bool IsEmpty() const {
        return num_operators == 0;
    }

    bool IsFull() const {
        return num_operators == max_operators;
    }

    gcc_pure
    bool IsRunning(const Plan *plan) const {
        for (const auto &i : operators)
            if (i->plan == plan)
                return true;

        return false;
    }

    gcc_pure
    const char *GetRunningPlanNames();

    /**
     * Returns the plan names which have reached their concurrency
     * limit.
     */
    gcc_pure
    const char *GetFullPlanNames();

    int Start(Job *job, Plan *plan);

    gcc_pure
    Workplace::OperatorList::iterator FindByPid(pid_t pid);

    void WaitPid();
};

#endif
