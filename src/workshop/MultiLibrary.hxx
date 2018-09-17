/*
 * Copyright 2006-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
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

    bool Update(std::chrono::steady_clock::time_point now, bool force) {
        bool modified = false;
        for (auto &i : libraries)
            if (i.Update(now, force))
                modified = true;
        return modified;
    }

    template<typename F>
    void VisitPlans(std::chrono::steady_clock::time_point now, F &&f) const {
        for (const auto &i : libraries)
            i.VisitPlans(now, f);
    }

    std::shared_ptr<Plan> Get(std::chrono::steady_clock::time_point now,
                              const char *name);
};

#endif
