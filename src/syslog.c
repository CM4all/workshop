/*
 * Syslog network client.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "syslog.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

struct syslog_client {
    int fd;
    char *me, *ident;
    int facility;
};

static int getaddrinfo_helper(const char *host_and_port, const char *default_port,
                              const struct addrinfo *hints,
                              struct addrinfo **ai_r) {
    const char *colon, *host, *port;
    char buffer[256];

    colon = strchr(host_and_port, ':');
    if (colon == NULL) {
        host = host_and_port;
        port = default_port;
    } else {
        size_t len = colon - host_and_port;

        if (len >= sizeof(buffer)) {
            errno = ENAMETOOLONG;
            return EAI_SYSTEM;
        }

        memcpy(buffer, host_and_port, len);
        buffer[len] = 0;

        host = buffer;
        port = colon + 1;
    }

    if (strcmp(host, "*") == 0)
        host = "0.0.0.0";

    return getaddrinfo(host, port, hints, ai_r);
}

int syslog_open(const char *me, const char *ident,
                int facility,
                const char *host_and_port,
                struct syslog_client **syslog_r) {
    int ret;
    struct addrinfo hints, *ai;
    struct syslog_client *syslog;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    ret = getaddrinfo_helper(host_and_port, "syslog", &hints, &ai);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo('%s') failed: %s\n",
                host_and_port, gai_strerror(ret));
        return -1;
    }

    syslog = calloc(1, sizeof(*syslog));
    if (syslog == NULL) {
        freeaddrinfo(ai);
        return ENOMEM;
    }

    syslog->fd = socket(ai->ai_family, ai->ai_socktype, SOCK_CLOEXEC);
    if (syslog->fd < 0) {
        int save_errno = errno;
        syslog_close(&syslog);
        freeaddrinfo(ai);
        return save_errno;
    }

    ret = connect(syslog->fd, ai->ai_addr, ai->ai_addrlen);
    freeaddrinfo(ai);
    if (ret < 0) {
        int save_errno = errno;
        syslog_close(&syslog);
        return save_errno;
    }

    syslog->me = strdup(me);
    syslog->ident = strdup(ident);
    if (syslog->me == NULL || syslog->ident == NULL) {
        int save_errno = errno;
        syslog_close(&syslog);
        return save_errno;
    }

    syslog->facility = facility;

    *syslog_r = syslog;
    return 0;
}

void syslog_close(struct syslog_client **syslog_r) {
    struct syslog_client *syslog;

    assert(syslog_r != NULL);
    assert(*syslog_r != NULL);

    syslog = *syslog_r;
    *syslog_r = NULL;

    if (syslog->fd >= 0)
        close(syslog->fd);

    if (syslog->me != NULL)
        free(syslog->me);

    if (syslog->ident != NULL)
        free(syslog->ident);

    free(syslog);
}

/** hack to put const char* into struct iovec */
static inline void *deconst(const char *p) {
    union {
        const char *in;
        void *out;
    } u = { .in = p };
    return u.out;
}

int syslog_log(struct syslog_client *syslog, int priority, const char *msg) {
    static const char space = ' ';
    static const char newline = '\n';
    static const char colon[] = ": ";
    char code[16];
    struct iovec iovec[] = {
        { .iov_base = code },
        { .iov_base = syslog->me, .iov_len = strlen(syslog->me) },
        { .iov_base = deconst(&space), .iov_len = 1 },
        { .iov_base = syslog->ident, .iov_len = strlen(syslog->ident) },
        { .iov_base = deconst(colon), .iov_len = strlen(colon) },
        { .iov_base = deconst(msg), .iov_len = strlen(msg) },
        { .iov_base = deconst(&newline), .iov_len = 1 },
    };
    ssize_t nbytes;

    assert(syslog != NULL);
    assert(syslog->fd >= 0);
    assert(priority >= 0 && priority < 8);

    snprintf(code, sizeof(code), "<%d>", syslog->facility * 8 + priority);
    iovec[0].iov_len = strlen(code);

    nbytes = writev(syslog->fd, iovec, sizeof(iovec) / sizeof(iovec[0]));
    if (nbytes < 0)
        return errno;

    if (nbytes == 0)
        return -1;

    return 0;
}
