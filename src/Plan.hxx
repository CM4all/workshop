/*
 * Internal declarations of cm4all-workshop.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_PLAN_HXX
#define WORKSHOP_PLAN_HXX

#include <string>
#include <list>

#include <string>
#include <vector>

#include <assert.h>
#include <sys/types.h>
#include <stdlib.h>

/** a library is a container for plan objects */
class Library;

/** a plan describes how to perform a specific job */
struct Plan {
    Library *library;

    std::list<std::string> args;

    std::string timeout, chroot;

    uid_t uid;
    gid_t gid;

    /** supplementary group ids */
    std::vector<gid_t> groups;

    int priority;

    /** maximum concurrency for this plan */
    unsigned concurrency;

    unsigned ref;

    Plan()
        :library(NULL),
         uid(65534), gid(65534),
         priority(10),
         concurrency(0),
         ref(0) {
    }

    Plan(const Plan &other) = delete;

    ~Plan() {
        assert(ref == 0);
    }

    Plan &operator=(const Plan &other) = delete;
};

int
library_open(const char *path, Library **library_r);

int
library_update(Library *library);

const char *
library_plan_names(Library *library);

int
library_get(Library *library, const char *name, Plan **plan_r);

void
plan_put(Plan **plan_r);

#endif
