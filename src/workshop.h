/*
 * Internal declarations of cm4all-workshop.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include <daemon/log.h>

#include <sys/types.h>
#include <event.h>


#ifdef NDEBUG
static const int debug_mode = 0;
#else
extern int debug_mode;
#endif


/* config.c */

struct config {
    const char *node_name;
    unsigned concurrency;
    const char *database;
};

/** read configuration options from the command line */
void parse_cmdline(struct config *config, int argc, char **argv);

void config_dispose(struct config *config);
