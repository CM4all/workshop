/*
 * $Id$
 *
 * Internal header for the plan library.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef __PLAN_INTERNAL_H
#define __PLAN_INTERNAL_H

struct plan;

void plan_free(struct plan **plan_r);

int plan_load(const char *path, struct plan **plan_r);

#endif
