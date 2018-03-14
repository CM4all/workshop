/*
 * Copyright 2006-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "CronGlue.hxx"
#include "CronClient.hxx"
#include "translation/Response.hxx"
#include "AllocatorPtr.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "system/Error.hxx"
#include "util/ScopeExit.hxx"

#include <sys/socket.h>
#include <unistd.h>

TranslateResponse
TranslateCron(AllocatorPtr alloc, const char *socket_path,
              const char *partition_name, const char *listener_tag,
              const char *user, const char *uri, const char *param)
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

    return TranslateCron(alloc, fd, partition_name, listener_tag,
                         user, uri, param);
}
