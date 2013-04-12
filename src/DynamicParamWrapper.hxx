/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SNOWBALL_DYNAMIC_PARAM_WRAPPER_HXX
#define SNOWBALL_DYNAMIC_PARAM_WRAPPER_HXX

#include "ParamWrapper.hxx"

#include <inline/compiler.h>

#include <vector>
#include <cstddef>
#include <cstdio>

template<typename T>
struct DynamicParamWrapper {
    ParamWrapper<T> wrapper;

    DynamicParamWrapper(const T &t):wrapper(t) {}

    constexpr static size_t Count(gcc_unused const T &t) {
        return 1;
    }

    template<typename O>
    unsigned Fill(O output) const {
        *output = wrapper.GetValue();
        return 1;
    }
};

template<typename T>
struct DynamicParamWrapper<std::vector<T>> {
    std::vector<DynamicParamWrapper<T>> items;

    constexpr DynamicParamWrapper(const std::vector<T> &params)
        :items(params.begin(), params.end()) {}

    constexpr static size_t Count(gcc_unused const std::vector<T> &v) {
        return v.size();
    }

    template<typename O>
    unsigned Fill(O output) const {
        unsigned total = 0;
        for (const auto &i : items) {
            const unsigned n = i.Fill(output);
            std::advance(output, n);
            total += n;
        }

        return total;
    }
};

#endif
