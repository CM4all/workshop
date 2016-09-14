/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Expand.hxx"

void
Expand(std::string &p, const StringMap &vars)
{
    const std::string src = std::move(p);
    p.clear();

    std::string::size_type start = 0, pos;
    while ((pos = src.find("${", start)) != src.npos) {
        std::string::size_type end = src.find('}', start + 2);
        if (end == src.npos)
            break;

        p.append(src.begin() + start, src.begin() + pos);

        const std::string key(src.begin() + start + 2, src.begin() + end);
        auto i = vars.find(key);
        if (i != vars.end())
            p.append(i->second);

        start = end + 1;
    }

    p.append(src.begin() + start, src.end());
}
