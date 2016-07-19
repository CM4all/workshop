/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_ALLOCATOR_PTR_HXX
#define WORKSHOP_ALLOCATOR_PTR_HXX

#include <forward_list>
#include <functional>
#include <new>

#include <stdlib.h>
#include <string.h>

struct StringView;

class Allocator {
    std::forward_list<std::function<void()>> cleanup;

public:
    ~Allocator() {
        for (auto &i : cleanup)
            i();
    }

    void *Allocate(size_t size) {
        void *p = malloc(size);
        if (p == nullptr)
            throw std::bad_alloc();

        cleanup.emplace_front([p](){ free(p); });
        return p;
    }

    char *Dup(const char *src) {
        char *p = strdup(src);
        if (p == nullptr)
            throw std::bad_alloc();

        cleanup.emplace_front([p](){ free(p); });
        return p;
    }

    template<typename T, typename... Args>
    T *New(Args&&... args) {
        auto p = new T(std::forward<Args>(args)...);
        cleanup.emplace_front([p](){ delete p; });
        return p;
    }

    template<typename T>
    T *NewArray(size_t n) {
        auto p = new T[n];
        cleanup.emplace_front([p](){ delete[] p; });
        return p;
    }
};

class AllocatorPtr {
    Allocator &allocator;

public:
    AllocatorPtr(Allocator &_allocator):allocator(_allocator) {}

    const char *Dup(const char *src) {
        return allocator.Dup(src);
    }

    const char *CheckDup(const char *src) {
        return src != nullptr ? allocator.Dup(src) : nullptr;
    }

    template<typename T, typename... Args>
    T *New(Args&&... args) {
        return allocator.New<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    T *NewArray(size_t n) {
        return allocator.NewArray<T>(n);
    }

    void *Dup(const void *data, size_t size) {
        auto p = allocator.Allocate(size);
        memcpy(p, data, size);
        return p;
    }

    StringView Dup(StringView src);
    const char *DupZ(StringView src);
};

#endif
