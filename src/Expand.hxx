/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_EXPAND_HXX
#define WORKSHOP_EXPAND_HXX

#include <map>
#include <string>

typedef std::map<std::string, std::string> StringMap;

void
Expand(std::string &p, const StringMap &vars);

#endif
