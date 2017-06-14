/*
 * Internal declarations of cm4all-workshop.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_PLAN_HXX
#define WORKSHOP_PLAN_HXX

#include <string>
#include <list>
#include <vector>
#include <chrono>

#include <assert.h>
#include <sys/types.h>
#include <stdlib.h>

class Error;

/** a plan describes how to perform a specific job */
struct Plan {
    std::list<std::string> args;

    std::string timeout, chroot;

    std::chrono::steady_clock::duration parsed_timeout =
        std::chrono::seconds(0);

    uid_t uid = 65534;
    gid_t gid = 65534;

    /** supplementary group ids */
    std::vector<gid_t> groups;

    int priority = 10;

    /** maximum concurrency for this plan */
    unsigned concurrency = 0;

    Plan() = default;

    Plan(Plan &&) = default;

    Plan(const Plan &other) = delete;

    Plan &operator=(Plan &&other) = default;
    Plan &operator=(const Plan &other) = delete;

    const std::string &GetExecutablePath() const {
        assert(!args.empty());

        return args.front();
    }
};

#endif
