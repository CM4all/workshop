/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "CronGlue.hxx"
#include "CronClient.hxx"
#include "Response.hxx"
#include "AllocatorPtr.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "system/Error.hxx"
#include "util/ScopeExit.hxx"

#include <sys/socket.h>
#include <unistd.h>

TranslateResponse
TranslateCron(AllocatorPtr alloc, const char *socket_path,
              const char *user, const char *param)
{
    int fd = socket(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0);
    if (fd < 0)
        throw MakeErrno("Failed to create translation socket");

    AtScopeExit(fd) { close(fd); };

    {
        AllocatedSocketAddress address;
        address.SetLocal(socket_path);

        if (connect(fd, address.GetAddress(), address.GetSize()) < 0)
            throw MakeErrno("Failed to connect to translation server");
    }

    return TranslateCron(alloc, fd, user, param);
}
