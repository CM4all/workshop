/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef BENG_PROXY_TRANSLATE_CRON_CLIENT_HXX
#define BENG_PROXY_TRANSLATE_CRON_CLIENT_HXX

struct TranslateResponse;
class AllocatorPtr;

TranslateResponse
TranslateCron(AllocatorPtr alloc, int fd,
              const char *partition_name, const char *listener_tag,
              const char *user, const char *uri,
              const char *param);

#endif
