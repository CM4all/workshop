/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "SyslogBridge.hxx"
#include "util/StringView.hxx"

void
SyslogBridge::Feed(const void *_data, size_t size)
{
    const auto data = (const char *)_data;

    for (size_t i = 0; i < size; ++i) {
        char ch = data[i];

        if (ch == '\r' || ch == '\n') {
            if (!buffer.empty()) {
                client.Log(6, {buffer.begin(), buffer.size()});
            }

            buffer.clear();
        } else if (ch > 0 && (ch & ~0x7f) == 0 &&
                   !buffer.full()) {
            buffer.push_back(ch);
        }
    }
}
