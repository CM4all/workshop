/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_MULTI_LIBRARY_HXX
#define WORKSHOP_MULTI_LIBRARY_HXX

#include "Library.hxx"

#include <memory>
#include <forward_list>

/**
 * Manages several #Library instances as one.
 */
class MultiLibrary {
    std::forward_list<Library> libraries;

public:
    void InsertPath(boost::filesystem::path &&_path) {
        libraries.emplace_front(std::move(_path));
    }

    bool Update(bool force) {
        bool modified = false;
        for (auto &i : libraries)
            if (i.Update(force))
                modified = true;
        return modified;
    }

    template<typename F>
    void VisitPlans(std::chrono::steady_clock::time_point now, F &&f) const {
        for (const auto &i : libraries)
            i.VisitPlans(now, f);
    }

    std::shared_ptr<Plan> Get(const char *name);
};

#endif
