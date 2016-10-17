/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Config.hxx"

#include <string.h>

CronConfig::CronConfig()
{
    memset(&user, 0, sizeof(user));

    spawn.default_uid_gid.uid = 65534;
    spawn.default_uid_gid.gid = 65534;
}
