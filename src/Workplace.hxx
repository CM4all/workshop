/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_WORKPLACE_HXX
#define WORKSHOP_WORKPLACE_HXX

#include "Operator.hxx"

#include <inline/compiler.h>

#include <string>
#include <list>

#include <assert.h>

struct Plan;
struct Job;
struct Operator;

class Workplace {
    std::string node_name;

    typedef std::list<Operator *> OperatorList;
    OperatorList operators;

    unsigned max_operators, num_operators;

public:
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

    gcc_pure
    const char *GetNodeName() const {
        return node_name.c_str();
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
    std::string GetRunningPlanNames() const;

    /**
     * Returns the plan names which have reached their concurrency
     * limit.
     */
    gcc_pure
    std::string GetFullPlanNames() const;

    int Start(Job *job, Plan *plan);

private:
    gcc_pure
    Workplace::OperatorList::iterator FindByPid(pid_t pid);

public:
    void WaitPid();
};

#endif
