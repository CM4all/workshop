/*
 * Dynamic string array.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef __STRARRAY_H
#define __STRARRAY_H

struct strarray {
    unsigned num, max;
    char **values;
};

#ifdef __cplusplus
extern  "C" {
#endif

void strarray_init(struct strarray *a);

void strarray_free(struct strarray *a);

void strarray_append(struct strarray *a, const char *v);

void strarray_remove(struct strarray *a, unsigned idx);

int strarray_index(struct strarray *a, const char *v);

int strarray_contains(struct strarray *a, const char *v);

#ifdef __cplusplus
}
#endif

#endif
