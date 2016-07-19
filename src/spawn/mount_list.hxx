/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef BENG_PROXY_MOUNT_LIST_HXX
#define BENG_PROXY_MOUNT_LIST_HXX

#include <inline/compiler.h>

class AllocatorPtr;
class MatchInfo;
class Error;

struct MountList {
    MountList *next;

    const char *source;
    const char *target;

    bool expand_source;

    bool writable;

    constexpr MountList(const char *_source, const char *_target,
                        bool _expand_source=false, bool _writable=false)
        :next(nullptr), source(_source), target(_target),
         expand_source(_expand_source), writable(_writable) {}

    MountList(AllocatorPtr alloc, const MountList &src);

    bool IsExpandable() const {
        return expand_source;
    }

    gcc_pure
    static bool IsAnyExpandable(MountList *m) {
        for (; m != nullptr; m = m->next)
            if (m->IsExpandable())
                return true;

        return false;
    }

    void Apply() const;

    static MountList *CloneAll(AllocatorPtr alloc, const MountList *src);
    static void ApplyAll(const MountList *m);
};

#endif
