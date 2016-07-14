/*
 * Syslog network client.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "SyslogClient.hxx"

#include <string>

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

class SyslogClient {
    int fd;
    const std::string me, ident;
    const int facility;

public:
    SyslogClient(int _fd, const char *_me, const char *_ident, int _facility)
        :fd(_fd), me(_me), ident(_ident), facility(_facility) {}

    SyslogClient(SyslogClient &&src)
        :fd(src.fd), me(std::move(src.me)), ident(std::move(src.ident)),
         facility(src.facility) {
        src.fd = -1;
    }

    ~SyslogClient() {
        if (fd >= 0)
            close(fd);
    }

    int Log(int priority, const char *msg);
};

static int getaddrinfo_helper(const char *host_and_port, const char *default_port,
                              const struct addrinfo *hints,
                              struct addrinfo **ai_r) {
    const char *colon, *host, *port;
    char buffer[256];

    colon = strchr(host_and_port, ':');
    if (colon == nullptr) {
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
                SyslogClient **syslog_r) {
    int ret;
    struct addrinfo hints, *ai;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    ret = getaddrinfo_helper(host_and_port, "syslog", &hints, &ai);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo('%s') failed: %s\n",
                host_and_port, gai_strerror(ret));
        return -1;
    }

    const int fd = socket(ai->ai_family, ai->ai_socktype, 0);
    if (fd < 0) {
        int save_errno = errno;
        freeaddrinfo(ai);
        return save_errno;
    }

    ret = connect(fd, ai->ai_addr, ai->ai_addrlen);
    freeaddrinfo(ai);
    if (ret < 0) {
        int save_errno = errno;
        close(fd);
        return save_errno;
    }

    *syslog_r = new SyslogClient(fd, me, ident, facility);
    return 0;
}

void syslog_close(SyslogClient **syslog_r) {
    SyslogClient *syslog;

    assert(syslog_r != nullptr);
    assert(*syslog_r != nullptr);

    syslog = *syslog_r;
    *syslog_r = nullptr;

    delete syslog;
}

/** hack to put const char* into struct iovec */
static inline void *deconst(const char *p) {
    union {
        const char *in;
        void *out;
    } u = { .in = p };
    return u.out;
}

int
SyslogClient::Log(int priority, const char *msg)
{
    static const char space = ' ';
    static const char newline = '\n';
    static const char colon[] = ": ";
    char code[16];
    struct iovec iovec[] = {
        { .iov_base = code },
        { .iov_base = deconst(me.c_str()), .iov_len = me.length() },
        { .iov_base = deconst(&space), .iov_len = 1 },
        { .iov_base = deconst(ident.c_str()), .iov_len = ident.length() },
        { .iov_base = deconst(colon), .iov_len = strlen(colon) },
        { .iov_base = deconst(msg), .iov_len = strlen(msg) },
        { .iov_base = deconst(&newline), .iov_len = 1 },
    };
    ssize_t nbytes;

    assert(fd >= 0);
    assert(priority >= 0 && priority < 8);

    snprintf(code, sizeof(code), "<%d>", facility * 8 + priority);
    iovec[0].iov_len = strlen(code);

    nbytes = writev(fd, iovec, sizeof(iovec) / sizeof(iovec[0]));
    if (nbytes < 0)
        return errno;

    if (nbytes == 0)
        return -1;

    return 0;
}

int syslog_log(SyslogClient *syslog, int priority, const char *msg) {
    assert(syslog != nullptr);

    return syslog->Log(priority, msg);
}
