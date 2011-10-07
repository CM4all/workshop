/*
 * Configure the workshop daemon.  Currently this is only command
 * line.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_CMDLINE_H
#define WORKSHOP_CMDLINE_H

struct config {
    const char *node_name;
    unsigned concurrency;
    const char *database;
};

/** read configuration options from the command line */
void parse_cmdline(struct config *config, int argc, char **argv);

void config_dispose(struct config *config);

#endif
