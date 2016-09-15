/*
 * Configure the workshop daemon.  Currently this is only command
 * line.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_COMMAND_LINE_HXX
#define WORKSHOP_COMMAND_LINE_HXX

#include <daemon/user.h>

struct Config {
    struct daemon_user user;

    const char *node_name = nullptr;
    unsigned concurrency = 2;
    const char *database = nullptr;

    Config();
};

/** read configuration options from the command line */
void
parse_cmdline(Config &config, int argc, char **argv);

#endif
