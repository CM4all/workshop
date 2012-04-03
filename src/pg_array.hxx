/*
 * Small utilities for PostgreSQL clients.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_PG_ARRAY_HXX
#define WORKSHOP_PG_ARRAY_HXX

#include <list>
#include <string>

bool
pg_decode_array(const char *p, std::list<std::string> &dest);

std::string
pg_encode_array(const std::list<std::string> &src);

#endif
