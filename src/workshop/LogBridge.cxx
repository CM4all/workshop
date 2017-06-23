/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "LogBridge.hxx"
#include "util/StringView.hxx"

bool
LogBridge::OnStderrLine(WritableBuffer<char> line)
{
    if (line.IsNull())
        return false;

    // TODO: strip non-ASCII characters
    client.Log(6, {line.data, line.size});
    return true;
}
