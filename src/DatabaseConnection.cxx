/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "DatabaseConnection.hxx"

#include <string.h>

std::string
DatabaseConnection::Escape(const char *p, size_t length) const
{
    assert(p != NULL || length == 0);

    char *buffer = new char[length * 2 + 1];

    ::PQescapeStringConn(conn, buffer, p, length, NULL);
    std::string result(buffer, length);
    delete[] buffer;
    return result;
}

std::string
DatabaseConnection::Escape(const char *p) const
{
    assert(p != NULL);

    return Escape(p, strlen(p));
}
