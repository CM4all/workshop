/*
 * $Id$
 *
 * Small utilities for PostgreSQL clients.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "pg-util.h"
#include "strarray.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>

int pg_decode_array(const char *p, struct strarray *a) {
    if (p == NULL || *p == 0)
        return 0;

    if (*p != '{')
        return -1;

    if (p[1] == '}' && p[2] == 0)
        return 0; /* special case: empty array */

    do {
        ++p;

        if (*p == '\"') {
            char *v, *w;

            ++p;

            v = w = (char*)malloc(strlen(p)); /* big enough */

            while (*p != '\"') {
                if (*p == '\\') {
                    ++p;

                    if (*p == 0) {
                        free(v);
                        return -1;
                    }

                    *w++ = *p++;
                } else if (*p == 0) {
                    free(v);
                    return -1;
                } else {
                    *w++ = *p++;
                }
            }

            ++p;

            if (*p != '}' && *p != ',') {
                free(v);
                return -1;
            }

            *w++ = 0;

            w = (char*)realloc(v, w - v);
            if (w != NULL)
                v = w;

            strarray_append(a, v);
            free(v);
        } else if (*p == 0 || *p == '{') {
            return -1;
        } else {
            const char *end;
            char *v;

            end = strchr(p, ',');
            if (end == NULL) {
                end = strchr(p, '}');
                if (end == NULL)
                    return -1;
            }

            v = (char*)malloc(end - p + 1);
            if (v == NULL)
                return ENOMEM;

            memcpy(v, p, end - p);
            v[end - p] = 0;

            strarray_append(a, v);
            free(v);

            p = end;
        }
    } while (*p == ',');

    if (*p != '}')
        return -1;

    ++p;

    if (*p != 0)
        return -1;

    return 0;
}

char *pg_encode_array(const struct strarray *a) {
    size_t max_length = 3;
    unsigned i;
    char *result, *p;
    const char *src;

    if (a == NULL)
        return NULL;

    if (a->num == 0)
        return strdup("{}");

    for (i = 0; i < a->num; ++i)
        max_length += 3 + 2 * strlen(a->values[i]);

    result = p = (char*)malloc(max_length);

    *p++ = '{';

    for (i = 0; i < a->num; ++i) {
        if (i > 0)
            *p++ = ',';
        *p++ = '"';
        src = a->values[i];
        while (*src) {
            if (*src == '\\' || *src == '"')
                *p++ = '\\';
            *p++ = *src++;
        }
        *p++ = '"';
    }

    *p++ = '}';
    *p++ = 0;

    return result;
}
