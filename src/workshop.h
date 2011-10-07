/*
 * Internal declarations of cm4all-workshop.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include <daemon/log.h>

#include <sys/types.h>
#include <event.h>

/* config.c */

struct config {
    const char *node_name;
    unsigned concurrency;
    const char *database;
};

/** read configuration options from the command line */
void parse_cmdline(struct config *config, int argc, char **argv);

void config_dispose(struct config *config);
