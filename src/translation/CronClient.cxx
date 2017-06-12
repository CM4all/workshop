/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "CronClient.hxx"
#include "translation/Parser.hxx"
#include "AllocatorPtr.hxx"
#include "system/Error.hxx"
#include "util/StaticFifoBuffer.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringView.hxx"

#include <beng-proxy/translation.h>

#include <stdexcept>

#include <string.h>
#include <sys/socket.h>

// TODO: remove this macro once beng-proxy 12 is out
#define TRANSLATE_CRON beng_translation_command(181)

static void
WriteHeader(void *&p, enum beng_translation_command command, size_t size)
{
    struct beng_translation_header header;
    header.length = (uint16_t)size;
    header.command = (uint16_t)command;

    p = mempcpy(p, &header, sizeof(header));
}

static void
WritePacket(void *&p, enum beng_translation_command cmd)
{
    WriteHeader(p, cmd, 0);
}

static void
WritePacket(void *&p, enum beng_translation_command cmd,
            ConstBuffer<void> payload)
{
    WriteHeader(p, cmd, payload.size);
    p = mempcpy(p, payload.data, payload.size);
}

static void
WritePacket(void *&p, enum beng_translation_command cmd,
            StringView payload)
{
    WritePacket(p, cmd, payload.ToVoid());
}

static void
SendFull(int fd, ConstBuffer<void> buffer)
{
    ssize_t nbytes = send(fd, buffer.data, buffer.size, MSG_NOSIGNAL);
    if (nbytes < 0)
        throw MakeErrno("send() to translation server failed");

    if (size_t(nbytes) != buffer.size)
        throw std::runtime_error("Short send() to translation server");
}

static void
SendTranslateCron(int fd, const char *partition_name,
                  const char *user, const char *uri, const char *param)
{
    assert(user != nullptr);

    if (strlen(user) > 256)
        throw std::runtime_error("User name too long");

    if (param != nullptr && strlen(param) > 4096)
        throw std::runtime_error("Translation parameter too long");

    char buffer[8192];
    void *p = buffer;

    WritePacket(p, TRANSLATE_BEGIN);

    size_t partition_name_size = partition_name != nullptr
        ? strlen(partition_name)
        : 0;
    WritePacket(p, TRANSLATE_CRON,
                StringView(partition_name, partition_name_size));

    WritePacket(p, TRANSLATE_USER, user);
    if (uri != nullptr)
        WritePacket(p, TRANSLATE_URI, uri);
    if (param != nullptr)
        WritePacket(p, TRANSLATE_PARAM, param);
    WritePacket(p, TRANSLATE_END);

    const size_t size = (char *)p - buffer;
    SendFull(fd, {buffer, size});
}

static TranslateResponse
ReceiveResponse(AllocatorPtr alloc, int fd)
{
    TranslateParser parser(alloc);

    StaticFifoBuffer<uint8_t, 8192> buffer;

    while (true) {
        auto w = buffer.Write();
        if (w.IsEmpty())
            throw std::runtime_error("Translation receive buffer is full");

        ssize_t nbytes = recv(fd, w.data, w.size, MSG_NOSIGNAL);
        if (nbytes < 0)
            throw MakeErrno("recv() from translation server failed");

        if (nbytes == 0)
            throw std::runtime_error("Translation server hung up");

        buffer.Append(nbytes);

        while (true) {
            auto r = buffer.Read();
            if (r.IsEmpty())
                break;

            size_t consumed = parser.Feed(r.data, r.size);
            if (consumed == 0)
                break;

            buffer.Consume(consumed);

            auto result = parser.Process();
            switch (result) {
            case TranslateParser::Result::MORE:
                break;

            case TranslateParser::Result::DONE:
                if (!buffer.IsEmpty())
                    throw std::runtime_error("Excessive data from translation server");

                return std::move(parser.GetResponse());
            }
        }
    }
}

TranslateResponse
TranslateCron(AllocatorPtr alloc, int fd,
              const char *partition_name,
              const char *user, const char *uri,
              const char *param)
{
    SendTranslateCron(fd, partition_name, user, uri, param);
    return ReceiveResponse(alloc, fd);
}
