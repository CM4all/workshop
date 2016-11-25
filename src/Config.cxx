/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Config.hxx"
#include "debug.h"

#include <string.h>

Config::Config()
{
    memset(&user, 0, sizeof(user));

    if (debug_mode) {
        spawn.default_uid_gid.LoadEffective();
    } else {
        spawn.default_uid_gid.uid = 65534;
        spawn.default_uid_gid.gid = 65534;
    }
}

void
Config::Check()
{
    if (!debug_mode && !daemon_user_defined(&user))
        throw std::runtime_error("no user name specified (-u)");

    if (node_name.empty())
        throw std::runtime_error("no node name specified");

    if (database.empty())
        throw std::runtime_error("no WORKSHOP_DATABASE environment variable");
}
