/*
 * Syslog network client.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "SyslogClient.hxx"
#include "net/Resolver.hxx"
#include "net/AddressInfo.hxx"
#include "system/Error.hxx"

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

SyslogClient *
SyslogClient::Create(const char *me, const char *ident,
                     int facility,
                     const char *host_and_port)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    const auto ail = Resolve(host_and_port, 514, &hints);
    const auto &ai = ail.front();

    UniqueSocketDescriptor fd;
    if (!fd.Create(ai.GetFamily(), ai.GetType(), ai.GetProtocol()))
        throw MakeErrno("Failed to create socket");

    if (!fd.Connect(ai))
        throw MakeErrno("Failed to connect to syslog server");

    return new SyslogClient(std::move(fd), me, ident, facility);
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

    assert(fd.IsDefined());
    assert(priority >= 0 && priority < 8);

    snprintf(code, sizeof(code), "<%d>", facility * 8 + priority);
    iovec[0].iov_len = strlen(code);

    nbytes = writev(fd.Get(), iovec, sizeof(iovec) / sizeof(iovec[0]));
    if (nbytes < 0)
        return errno;

    if (nbytes == 0)
        return -1;

    return 0;
}
