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

#include "SyslogClient.hxx"
#include "net/RConnectSocket.hxx"
#include "net/AddressInfo.hxx"
#include "system/Error.hxx"
#include "util/StringView.hxx"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>

SyslogClient::SyslogClient(const char *host_and_port,
                           const char *_me, const char *_ident,
                           int _facility)
    :SyslogClient(ResolveConnectDatagramSocket(host_and_port, 514),
                  _me, _ident, _facility)
{
}

static constexpr struct iovec
MakeIovec(const void *data, size_t size)
{
    return { const_cast<void *>(data), size };
}

static constexpr struct iovec
MakeIovec(StringView value)
{
    return MakeIovec(value.data, value.size);
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
SyslogClient::Log(int priority, StringView msg)
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
