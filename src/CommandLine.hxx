/*
 * Configure the workshop daemon.  Currently this is only command
 * line.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_COMMAND_LINE_HXX
#define WORKSHOP_COMMAND_LINE_HXX

struct Config;

/** read configuration options from the command line */
void
ParseCommandLine(Config &config, int argc, char **argv);

#endif
