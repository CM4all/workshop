/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Config.hxx"

#include <string.h>

Config::Config()
{
    memset(&user, 0, sizeof(user));
}
