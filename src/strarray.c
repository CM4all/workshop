/*
 * Dynamic string array.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "strarray.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

void strarray_init(struct strarray *a) {
    assert(a != NULL);

    memset(a, 0, sizeof(*a));
}

void strarray_free(struct strarray *a) {
    assert(a != NULL);

    if (a->values != NULL) {
        unsigned i;

        for (i = 0; i < a->num; ++i)
            if (a->values[i] != NULL)
                free(a->values[i]);

        free(a->values);
    }

    memset(a, 0, sizeof(*a));
}

void strarray_append(struct strarray *a, const char *v) {
    if (a->num >= a->max) {
        a->max += 16;
        a->values = realloc(a->values, a->max * sizeof(a->values[0]));
        if (a->values == NULL)
            abort();
    }

    if (v == NULL) {
        a->values[a->num] = NULL;
    } else {
        a->values[a->num] = strdup(v);
        if (a->values[a->num] == NULL)
            abort();
    }

    ++a->num;
}

void strarray_remove(struct strarray *a, unsigned idx) {
    assert(idx < a->num);
    assert(a->values[idx] != NULL);

    free(a->values[idx]);
    --a->num;

    if (idx < a->num)
        memmove(&a->values[idx], &a->values[idx + 1],
                sizeof(&a->values[idx]) * (a->num - idx));

    a->values[a->num] = NULL;
}

int strarray_index(struct strarray *a, const char *v) {
    unsigned i;

    assert(v != NULL);

    for (i = 0; i < a->num; ++i)
        if (a->values[i] != NULL && strcmp(a->values[i], v) == 0)
            return (int)i;

    return -1;
}

int strarray_contains(struct strarray *a, const char *v) {
    return strarray_index(a, v) >= 0;
}
