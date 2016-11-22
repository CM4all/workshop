/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Config.hxx"
#include "debug.h"

#include <string.h>

CronConfig::CronConfig()
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
CronConfig::Check()
{
    if (!debug_mode && !daemon_user_defined(&user))
        throw std::runtime_error("no user name specified (-u)");

    if (node_name == nullptr)
        throw std::runtime_error("no node name specified");

    if (database == nullptr || *database == 0)
        throw std::runtime_error("no CRON_DATABASE environment variable");
}
