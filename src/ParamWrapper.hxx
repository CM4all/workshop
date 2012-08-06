/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SNOWBALL_PARAM_WRAPPER_HXX
#define SNOWBALL_PARAM_WRAPPER_HXX

#include <cstdio>

template<typename T>
struct ParamWrapper {
    ParamWrapper(const T &t);
    const char *operator()() const;
};

template<>
struct ParamWrapper<const char *> {
    const char *value;

    constexpr ParamWrapper(const char *_value):value(_value) {}

    constexpr const char *operator()() const {
        return value;
    }
};

template<>
struct ParamWrapper<int> {
    char buffer[16];

    ParamWrapper(int i) {
        sprintf(buffer, "%i", i);
    }

    const char *operator()() const {
        return buffer;
    }
};

template<>
struct ParamWrapper<unsigned> {
    char buffer[16];

    ParamWrapper(unsigned i) {
        sprintf(buffer, "%u", i);
    }

    const char *operator()() const {
        return buffer;
    }
};

template<>
struct ParamWrapper<bool> {
    const char *value;

    constexpr ParamWrapper(bool _value):value(_value ? "t" : "f") {}

    constexpr const char *operator()() const {
        return value;
    }
};

#endif
