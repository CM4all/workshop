/*
 * $Id$
 *
 * Small utilities for PostgreSQL clients.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

struct strarray;

int pg_decode_array(const char *p, struct strarray *a);
