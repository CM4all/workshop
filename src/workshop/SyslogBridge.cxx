/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "SyslogBridge.hxx"
#include "util/StringView.hxx"

bool
SyslogBridge::OnStderrLine(WritableBuffer<char> line)
{
    if (line.IsNull())
        return false;

    // TODO: strip non-ASCII characters
    client.Log(6, {line.data, line.size});
    return true;
}
