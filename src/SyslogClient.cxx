/*
 * Syslog network client.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "SyslogClient.hxx"
#include "util/ScopeExit.hxx"

#include <system_error>

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

SyslogClient::~SyslogClient()
{
    if (fd >= 0)
        close(fd);
}

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

SyslogClient *
SyslogClient::Create(const char *me, const char *ident,
                     int facility,
                     const char *host_and_port)
{
    int ret;
    struct addrinfo hints, *ai;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    ret = getaddrinfo_helper(host_and_port, "syslog", &hints, &ai);
    if (ret != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "getaddrinfo('%s') failed: %s",
                 host_and_port, gai_strerror(ret));
        throw std::runtime_error(msg);
    }

    AtScopeExit(ai) { freeaddrinfo(ai); };

    const int fd = socket(ai->ai_family, ai->ai_socktype, 0);
    if (fd < 0)
        throw std::system_error(std::error_code(errno, std::system_category()),
                                "Failed to create socket");

    ret = connect(fd, ai->ai_addr, ai->ai_addrlen);
    if (ret < 0) {
        AtScopeExit(fd) { close(fd); };
        throw std::system_error(std::error_code(errno, std::system_category()),
                                "Failed to connect to syslog server");
    }

    return new SyslogClient(fd, me, ident, facility);
}

static constexpr struct iovec
MakeIovec(const void *data, size_t size)
{
    return { const_cast<void *>(data), size };
}

static struct iovec
MakeIovec(const char *value)
{
    return MakeIovec(value, strlen(value));
}

static struct iovec
MakeIovec(const std::string &value)
{
    return MakeIovec(value.data(), value.length());
}

int
SyslogClient::Log(int priority, const char *msg)
{
    static const char space = ' ';
    static const char newline = '\n';
    static const char colon[] = ": ";
    char code[16];
    struct iovec iovec[] = {
        MakeIovec(code, 0),
        MakeIovec(me),
        MakeIovec(&space, sizeof(space)),
        MakeIovec(ident),
        MakeIovec(colon),
        MakeIovec(msg),
        MakeIovec(&newline, 1),
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
