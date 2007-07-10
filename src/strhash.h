/*
 * Dynamic string hash.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef __STRHASH_H
#define __STRHASH_H

struct strhash;

int strhash_open(unsigned num_slots, struct strhash **sh_r);

void strhash_close(struct strhash **sh_r);

int strhash_set(struct strhash *sh, const char *key, const char *value);

const char *strhash_get(struct strhash *sh, const char *key);

#endif
