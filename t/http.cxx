/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "curl/Request.hxx"
#include "curl/Handler.hxx"
#include "curl/Global.hxx"
#include "curl/Init.hxx"
#include "event/Loop.hxx"
#include "util/PrintException.hxx"

#include <stdexcept>

#include <stdio.h>
#include <stdlib.h>

class MyResponseHandler final : public CurlResponseHandler {
public:
    void OnHeaders(unsigned status,
                   std::multimap<std::string, std::string> &&headers) {
        printf("Status: %u\n", status);
        for (const auto &i : headers)
            printf("%s: %s\n", i.first.c_str(), i.second.c_str());
        printf("\n");
    }

    void OnData(ConstBuffer<void> data) {
        fwrite(data.data, 1, data.size, stdout);
    }

    void OnEnd() {
    }

    void OnError(std::exception_ptr ep) noexcept {
        try {
            std::rethrow_exception(ep);
        } catch (...) {
            PrintException(std::current_exception());
        }
    }
};

int
main(int argc, char **argv)
try {
    if (argc != 2)
        throw std::runtime_error("Usage: http URL");

    const char *const url = argv[1];

    ScopeCurlInit curl_init;

    EventLoop event_loop;
    CurlGlobal curl_global(event_loop);;

    MyResponseHandler handler;
    CurlRequest request(curl_global, url, handler);

    event_loop.Dispatch();

    return EXIT_SUCCESS;
} catch (...) {
    PrintException(std::current_exception());
    return EXIT_FAILURE;
}
