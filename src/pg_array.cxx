/*
 * Small utilities for PostgreSQL clients.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "pg_array.hxx"

#include <assert.h>
#include <string.h>

bool
pg_decode_array(const char *p, std::list<std::string> &dest)
{
    assert(dest.empty());

    if (p == NULL || *p == 0)
        return true;

    if (*p != '{')
        return false;

    if (p[1] == '}' && p[2] == 0)
        return true; /* special case: empty array */

    do {
        ++p;

        if (*p == '\"') {
            ++p;

            std::string value;

            while (*p != '\"') {
                if (*p == '\\') {
                    ++p;

                    if (*p == 0)
                        return false;

                    value.push_back(*p++);
                } else if (*p == 0) {
                    return false;
                } else {
                    value.push_back(*p++);
                }
            }

            ++p;

            if (*p != '}' && *p != ',')
                return false;;

            dest.push_back(std::move(value));
        } else if (*p == 0 || *p == '{') {
            return false;
        } else {
            const char *end = strchr(p, ',');
            if (end == NULL) {
                end = strchr(p, '}');
                if (end == NULL)
                    return false;
            }

            dest.push_back(std::string(p, end));

            p = end;
        }
    } while (*p == ',');

    if (*p != '}')
        return false;

    ++p;

    if (*p != 0)
        return false;

    return true;
}

std::string
pg_encode_array(const std::list<std::string> &src)
{
    if (src.empty())
        return "{}";

    std::string dest("{");

    bool first = true;
    for (const auto &i : src) {
        if (first)
            first = false;
        else
            dest.push_back(',');

        dest.push_back('"');

        for (const auto ch : i) {
            if (ch == '\\' || ch == '"')
                dest.push_back('\\');
            dest.push_back(ch);
        }

        dest.push_back('"');
    }

    dest.push_back('}');
    return dest;
}
