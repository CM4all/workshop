/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "mount_list.hxx"
#include "system/bind_mount.h"
#include "AllocatorPtr.hxx"

#include <sys/mount.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

inline
MountList::MountList(AllocatorPtr alloc, const MountList &src)
    :next(nullptr),
     source(alloc.Dup(src.source)),
     target(alloc.Dup(src.target)),
     expand_source(src.expand_source),
     writable(src.writable) {}

MountList *
MountList::CloneAll(AllocatorPtr alloc, const MountList *src)
{
    MountList *head = nullptr, **tail = &head;

    for (; src != nullptr; src = src->next) {
        MountList *dest = alloc.New<MountList>(alloc, *src);
        *tail = dest;
        tail = &dest->next;
    }

    return head;
}

inline void
MountList::Apply() const
{
    int flags = MS_NOEXEC|MS_NOSUID|MS_NODEV;
    if (!writable)
        flags |= MS_RDONLY;

    bind_mount(source, target, flags);
}

void
MountList::ApplyAll(const MountList *m)
{
    for (; m != nullptr; m = m->next)
        m->Apply();
}
