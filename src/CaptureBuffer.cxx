/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "CaptureBuffer.hxx"
#include "util/CharUtil.hxx"

#include <algorithm>

CaptureBuffer::CaptureBuffer(EventLoop &event_loop, UniqueFileDescriptor &&_fd)
        :fd(std::move(_fd)),
         event(event_loop, fd.Get(), EV_READ|EV_PERSIST,
               BIND_THIS_METHOD(OnSocket))
{
    event.Add();
}

CaptureBuffer::~CaptureBuffer()
{
    event.Delete();
}

void
CaptureBuffer::OnSocket(short)
{
    if (length < data.size()) {
        ssize_t nbytes = fd.Read(&data[length], data.size() - length);
        if (nbytes <= 0) {
            Close();
            return;
        }

        length += nbytes;
    } else {
        /* buffer is full: discard data to keep the pipe from blocking
           the other end */
        char discard[4096];
        if (fd.Read(discard, sizeof(discard)) <= 0)
            Close();
    }
}

static constexpr bool
IsAllowedNonPrintableChar(char ch)
{
    return ch == '\r' || ch == '\n' || ch == '\t';
}

static constexpr bool
IsDisallowedChar(char ch)
{
    return !IsPrintableASCII(ch) && !IsAllowedNonPrintableChar(ch);
}

char *
CaptureBuffer::NormalizeASCII()
{
    if (length == data.size())
        /* crop the last character to make room for the null
           terminator */
        length = data.size() - 1;

    std::replace_if(data.begin(), std::next(data.begin(), length),
                    IsDisallowedChar, ' ');
    data[length] = 0;
    return &data.front();
}
