/*
 * Configure the workshop daemon.  Currently this is only command
 * line.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "cmdline.h"
#include "debug.h"
#include "version.h"

#include <daemon/log.h>
#include <daemon/daemonize.h>

#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>

static void usage(void) {
    puts("usage: cm4all-workshop [options]\n\n"
         "valid options:\n"
         " -h             help (this text)\n"
#ifdef __GLIBC__
         " --version\n"
#endif
         " -V             show cm4all-workshop version\n"
#ifdef __GLIBC__
         " --verbose\n"
#endif
         " -v             be more verbose\n"
#ifdef __GLIBC__
         " --quiet\n"
#endif
         " -q             be quiet\n"
#ifdef __GLIBC__
         " --name NAME\n"
#endif
         " -N NAME        set the node name\n"
#ifdef __GLIBC__
         " --concurrency NUM\n"
#endif
         " -c NUM         set the maximum number of concurrent operators (default: 2)\n"
#ifdef __GLIBC__
         " --database CONNINFO\n"
#endif
         " -d CONNINFO    set the PostgreSQL connect string\n"
#ifdef __GLIBC__
         " --logger program\n"
#endif
         " -l program     specifies a logger program (executed by /bin/sh)\n"
         " -D             don't detach (daemonize)\n"
#ifdef __GLIBC__
         " --pidfile file\n"
#endif
         " -P file        create a pid file\n"
#ifdef __GLIBC__
         " --user name\n"
#endif
         " -u name        switch to another user id\n"
#ifdef __GLIBC__
         " --logger-user name\n"
#endif
         " -U name        execute the logger program with this user id\n"
         "\n"
         );
}

static void arg_error(const char *argv0, const char *fmt, ...)
     __attribute__ ((noreturn))
     __attribute__((format(printf,2,3)));
static void arg_error(const char *argv0, const char *fmt, ...) {
    if (fmt != NULL) {
        va_list ap;

        fputs(argv0, stderr);
        fputs(": ", stderr);

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);

        putc('\n', stderr);
    }

    fprintf(stderr, "Try '%s --help' for more information.\n",
            argv0);
    exit(1);
}

/** read configuration options from the command line */
void parse_cmdline(struct config *config, int argc, char **argv) {
    int ret;
#ifdef __GLIBC__
    static const struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"version", 0, 0, 'V'},
        {"verbose", 0, 0, 'v'},
        {"quiet", 0, 0, 'q'},
        {"name", 1, 0, 'N'},
        {"concurrency", 1, 0, 'c'},
        {"database", 1, 0, 'd'},
        {"logger", 1, 0, 'l'},
        {"pidfile", 1, 0, 'P'},
        {"user", 1, 0, 'u'},
        {"logger-user", 1, 0, 'U'},
        {0,0,0,0}
    };
#endif

    config->database = getenv("WORKSHOP_DATABASE");
    daemon_config.logger = getenv("WORKSHOP_LOGGER");

    while (1) {
#ifdef __GLIBC__
        int option_index = 0;

        ret = getopt_long(argc, argv, "hVvqN:c:d:DP:l:u:U:",
                          long_options, &option_index);
#else
        ret = getopt(argc, argv, "hVvqN:c:d:DP:l:u:U:");
#endif
        if (ret == -1)
            break;

        switch (ret) {
        case 'h':
            usage();
            exit(0);

        case 'V':
            printf("cm4all-workshop v%s\n", VERSION);
            exit(0);

        case 'v':
            ++daemon_log_config.verbose;
            break;

        case 'q':
            daemon_log_config.verbose = 0;
            break;

        case 'N':
            config->node_name = optarg;
            break;

        case 'c':
            config->concurrency = (unsigned)strtoul(optarg, NULL, 10);
            if (config->concurrency == 0)
                arg_error(argv[0], "invalid concurrency specification");
            break;

        case 'd':
            config->database = optarg;
            break;

        case 'D':
            daemon_config.detach = 0;
            break;

        case 'P':
            daemon_config.pidfile = optarg;
            break;

        case 'l':
            daemon_config.logger = optarg;
            break;

        case 'u':
            if (debug_mode)
                arg_error(argv[0], "cannot specify a user in debug mode");

            daemon_user_by_name(&daemon_config.user, optarg, NULL);
            daemon_config.user.real_uid_root = 1;
            if (!daemon_user_defined(&daemon_config.user))
                arg_error(argv[0], "refusing to run as root");
            break;

        case 'U':
            if (debug_mode)
                arg_error(argv[0], "cannot specify a user in debug mode");

            daemon_user_by_name(&daemon_config.logger_user, optarg, NULL);
            break;

        case '?':
            arg_error(argv[0], NULL);

        default:
            exit(1);
        }
    }

    /* check non-option arguments */

    if (optind < argc)
        arg_error(argv[0], "unrecognized argument: %s", argv[optind]);

    /* check completeness */

    if (config->node_name == NULL)
        arg_error(argv[0], "no node name specified");

    if (config->database == NULL)
        arg_error(argv[0], "no database specified");

    if (!debug_mode && !daemon_user_defined(&daemon_config.user))
        arg_error(argv[0], "no user name specified (-u)");
}

void
config_dispose(G_GNUC_UNUSED struct config *config)
{
}
