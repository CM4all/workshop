#include "strhash.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct pair {
    struct pair *next;
    char *key, *value;
};

struct strhash {
    struct pair **slots;
    unsigned num_slots;
};

static unsigned calc_hash(const char *p) {
    unsigned hash = 0;

    while (*p != 0)
        hash = (hash << 5) + hash + *p++;

    return hash;
}

int strhash_open(unsigned num_slots, struct strhash **sh_r) {
    struct strhash *sh;

    sh = calloc(1, sizeof(*sh));
    if (sh == NULL)
        return errno;

    sh->slots = calloc(num_slots, sizeof(sh->slots[0]));
    if (sh->slots == NULL) {
        strhash_close(&sh);
        return ENOMEM;
    }

    sh->num_slots = num_slots;

    *sh_r = sh;
    return 0;
}

static void free_pair(struct pair *p) {
    if (p->key != NULL)
        free(p->key);
    if (p->value != NULL)
        free(p->value);
    free(p);
}

void strhash_close(struct strhash **sh_r) {
    struct strhash *sh;
    unsigned i;
    struct pair *p, *next;

    assert(sh_r != NULL);
    assert(*sh_r != NULL);

    sh = *sh_r;
    *sh_r = NULL;

    for (i = 0; i < sh->num_slots; ++i) {
        for (p = sh->slots[i]; p != NULL; p = next) {
            next = p->next;
            free_pair(p);
        }
    }

    if (sh->slots != NULL)
        free(sh->slots);

    free(sh);
}

static struct pair **find_slot(struct pair **slot_p, const char *key) {
    struct pair *slot;

    while (*slot_p != NULL) {
        slot = *slot_p;

        assert(slot->key != NULL);

        if (strcmp(slot->key, key) == 0)
            return slot_p;

        slot_p = &slot->next;
    }

    return slot_p;
}

int strhash_set(struct strhash *sh, const char *key, const char *value) {
    unsigned hash = calc_hash(key) % sh->num_slots;
    struct pair **slot_p, *slot;
    char *v;

    assert(key != NULL && value != NULL);

    slot_p = find_slot(&sh->slots[hash], key);
    if (*slot_p == NULL) {
        slot = calloc(1, sizeof(*slot));
        if (slot == NULL)
            return errno;

        slot->key = strdup(key);
        slot->value = strdup(value);

        if (slot->key == NULL || slot->value == NULL) {
            free_pair(slot);
            return errno;
        }

        *slot_p = slot;
    } else {
        slot = *slot_p;

        assert(slot->value != NULL);

        v = strdup(value);
        if (v == NULL)
            return errno;

        free(slot->value);
        slot->value = v;
    }

    return 0;
}

const char *strhash_get(struct strhash *sh, const char *key) {
    unsigned hash = calc_hash(key) % sh->num_slots;
    struct pair **slot_p, *slot;

    slot_p = find_slot(&sh->slots[hash], key);
    if (*slot_p == NULL)
        return NULL;

    slot = *slot_p;
    assert(slot->value != NULL);

    return slot->value;
}

