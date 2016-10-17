/*
 * Configure the cron daemon.  Currently this is only command line.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CRON_COMMAND_LINE_HXX
#define CRON_COMMAND_LINE_HXX

struct CronConfig;

/** read configuration options from the command line */
void
CronParseCommandLine(CronConfig &config, int argc, char **argv);

#endif
