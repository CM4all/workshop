/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef BENG_PROXY_TRANSLATE_CRON_GLUE_HXX
#define BENG_PROXY_TRANSLATE_CRON_GLUE_HXX

struct TranslateResponse;
class AllocatorPtr;

TranslateResponse
TranslateCron(AllocatorPtr alloc, const char *socket_path,
              const char *partition_name, const char *listener_tag,
              const char *user, const char *uri, const char *param);

#endif
