/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_SYSLOG_BRIDGE_HXX
#define WORKSHOP_SYSLOG_BRIDGE_HXX

#include "SyslogClient.hxx"
#include "util/StaticArray.hxx"

class SyslogBridge {
    SyslogClient client;

    StaticArray<char, 64> buffer;

public:
    SyslogBridge(const char *host_and_port,
                 const char *me, const char *ident,
                 int facility)
        :client(host_and_port, me, ident, facility) {}

    void Feed(const void *data, size_t size);
};

#endif
