/*
 * Manage operator processes.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "ProgressReader.hxx"

ProgressReader::ProgressReader(EventLoop &event_loop,
                               UniqueFileDescriptor _fd,
                               Callback _callback)
    :fd(std::move(_fd)),
     event(event_loop, fd.Get(), SocketEvent::READ|SocketEvent::PERSIST,
           BIND_THIS_METHOD(PipeReady)),
     callback(_callback)
{
    event.Add();
}

void
ProgressReader::PipeReady(unsigned)
{
    char buffer[512];
    ssize_t nbytes, i;
    unsigned new_progress = 0, p;

    nbytes = fd.Read(buffer, sizeof(buffer));
    if (nbytes <= 0) {
        event.Delete();
        fd.Close();
        return;
    }

    for (i = 0; i < nbytes; ++i) {
        char ch = buffer[i];

        if (ch >= '0' && ch <= '9' &&
            stdout_buffer.size() < stdout_buffer.capacity() - 1) {
            stdout_buffer.push_back(ch);
        } else {
            if (!stdout_buffer.empty()) {
                stdout_buffer.push_back('\0');
                p = (unsigned)strtoul(stdout_buffer.begin(), nullptr, 10);
                if (p <= 100)
                    new_progress = p;
            }

            stdout_buffer.clear();
        }
    }

    if (new_progress > 0 && new_progress != last_progress) {
        callback(new_progress);
        last_progress = new_progress;
    }
}
