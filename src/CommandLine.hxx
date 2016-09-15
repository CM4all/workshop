/*
 * Configure the workshop daemon.  Currently this is only command
 * line.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_COMMAND_LINE_HXX
#define WORKSHOP_COMMAND_LINE_HXX

struct config {
    const char *node_name;
    unsigned concurrency;
    const char *database;
};

/** read configuration options from the command line */
void parse_cmdline(struct config *config, int argc, char **argv);

#endif
