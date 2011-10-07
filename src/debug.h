/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_DEBUG_H
#define WORKSHOP_DEBUG_H

#ifdef NDEBUG
static const int debug_mode = 0;
#else
extern int debug_mode;
#endif

#endif
