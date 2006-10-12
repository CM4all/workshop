/*
 * $Id$
 *
 * Framework for poll() with callbacks.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "workshop.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <poll.h>

struct info {
    poll_callback_t callback;
    void *ctx;
};

struct poll {
    /** structure passed to poll() */
    struct pollfd *fds;
    /** callback data */
    struct info *info;
    /** size of the two arrays */
    unsigned num, max;
};

int poll_open(struct poll **poll_r) {
    struct poll *p;

    p = (struct poll*)calloc(1, sizeof(*p));
    if (p == NULL)
        return errno;

    *poll_r = p;
    return 0;
}

void poll_close(struct poll **poll_r) {
    struct poll *p;

    assert(poll_r != NULL);
    assert(*poll_r != NULL);

    p = *poll_r;
    *poll_r = NULL;

    assert(p->num == 0);

    if (p->fds != NULL)
        free(p->fds);

    if (p->info != NULL)
        free(p->info);

    free(p);
}

void poll_add(struct poll *p, int fd, short events,
              poll_callback_t callback, void *ctx) {
    assert(p->num <= p->max);

    if (p->num >= p->max) {
        /* grow array */

        p->max += 16;

        p->fds = (struct pollfd*)realloc(p->fds, p->max * sizeof(p->fds[0]));
        p->info = (struct info*)realloc(p->info, p->max * sizeof(p->info[0]));
        if (p->fds == NULL || p->info == NULL)
            abort();
    }

    p->fds[p->num] = (struct pollfd) {
        .fd = fd,
        .events = events,
    };

    p->info[p->num] = (struct info) {
        .callback = callback,
        .ctx = ctx,
    };

    ++p->num;
}

static unsigned find_fd(struct poll *p, int fd) {
    unsigned i;

    for (i = 0; i < p->num; ++i)
        if (p->fds[i].fd == fd)
            return i;

    abort();
}

void poll_remove(struct poll *p, int fd) {
    unsigned i;

    i = find_fd(p, fd);

    --p->num;

    if (i < p->num) {
        p->fds[i] = p->fds[p->num];
        p->info[i] = p->info[p->num];
    }
}

void poll_poll(struct poll *p, int timeout) {
    int ret;
    unsigned i;

    assert(p->num > 0);

    /* poll all registered file handles */

    ret = poll(p->fds, p->num, timeout);
    assert(ret != 0);

    if (ret < 0) {
        if (errno == EINTR)
            return;

        fprintf(stderr, "poll() failed: %s\n", strerror(errno));
        exit(2);
    }

    /* execute callbacks */

    for (i = 0; ret > 0 && i < p->num; ++i) {
        if (p->fds[i].revents != 0) {
            p->info[i].callback(&p->fds[i], p->info[i].ctx);
            --ret;
        }
    }
}
