/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef BENG_PROXY_SPAWN_CONFIG_HXX
#define BENG_PROXY_SPAWN_CONFIG_HXX

#include "UidGid.hxx"

#include <inline/compiler.h>

#include <set>

/**
 * Configuration for the spawner.
 */
struct SpawnConfig {
    UidGid default_uid_gid;

    /**
     * Ignore the user namespaces setting?  This is used as a
     * workaround to allow the spawner run as root.
     *
     * TODO: replace this workaround
     */
    bool ignore_userns = false;

    gcc_pure
    bool Verify(const UidGid &uid_gid) const {
        // TODO: implement
        (void)uid_gid;
        return true;
    }
};

#endif
