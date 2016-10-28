/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_JOB_HXX
#define CRON_JOB_HXX

#include <string>

struct CronJob {
    std::string id, account_id, command, translate_param;
};

#endif
