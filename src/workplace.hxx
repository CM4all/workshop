/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_WORKPLACE_H
#define WORKSHOP_WORKPLACE_H

#include "operator.hxx"

#include <string>
#include <list>

#include <assert.h>

struct plan;
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

    bool IsRunning(const struct plan *plan) const {
        for (const auto &i : operators)
            if (i->plan == plan)
                return true;

        return false;
    }
};

Workplace *
workplace_open(const char *node_name, unsigned max_operators);

void
workplace_free(Workplace *workplace);

bool
workplace_plan_is_running(const Workplace *workplace, const struct plan *plan);

const char *
workplace_plan_names(Workplace *workplace);

/** returns the plan names which have reached their concurrency
    limit */
const char *
workplace_full_plan_names(Workplace *workplace);

int
workplace_start(Workplace *workplace, Job *job, struct plan *plan);

static inline bool
workplace_is_empty(const Workplace *workplace)
{
    return workplace->IsEmpty();
}

static inline bool
workplace_is_full(const Workplace *workplace)
{
    return workplace->IsFull();
}

void
workplace_waitpid(Workplace *workplace);

#endif
