/*
 * Configure the workshop daemon.  Currently this is only command
 * line.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "CommandLine.hxx"
#include "Config.hxx"
#include "debug.h"

#include <inline/compiler.h>
#include <daemon/log.h>

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
         " -D             don't detach (daemonize)\n"
#ifdef __GLIBC__
         " --user name\n"
#endif
         " -u name        switch to another user id\n"
         "\n"
         );
}

static void arg_error(const char *argv0, const char *fmt, ...)
     __attribute__ ((noreturn))
     __attribute__((format(printf,2,3)));
static void arg_error(const char *argv0, const char *fmt, ...) {
    if (fmt != nullptr) {
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
void
parse_cmdline(Config &config, int argc, char **argv)
{
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
        {"user", 1, 0, 'u'},
        {0,0,0,0}
    };
#endif

    config.database = getenv("WORKSHOP_DATABASE");

    while (1) {
#ifdef __GLIBC__
        int option_index = 0;

        ret = getopt_long(argc, argv, "hVvqN:c:d:Du:",
                          long_options, &option_index);
#else
        ret = getopt(argc, argv, "hVvqN:c:d:Du:");
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
            config.node_name = optarg;
            break;

        case 'c':
            config.concurrency = (unsigned)strtoul(optarg, nullptr, 10);
            if (config.concurrency == 0)
                arg_error(argv[0], "invalid concurrency specification");
            break;

        case 'd':
            config.database = optarg;
            break;

        case 'u':
            if (debug_mode)
                arg_error(argv[0], "cannot specify a user in debug mode");

            daemon_user_by_name(&config.user, optarg, nullptr);
            if (!daemon_user_defined(&config.user))
                arg_error(argv[0], "refusing to run as root");
            break;

        case '?':
            arg_error(argv[0], nullptr);

        default:
            exit(1);
        }
    }

    /* check non-option arguments */

    if (optind < argc)
        arg_error(argv[0], "unrecognized argument: %s", argv[optind]);

    /* check completeness */

    if (config.node_name == nullptr)
        arg_error(argv[0], "no node name specified");

    if (config.database == nullptr)
        arg_error(argv[0], "no database specified");

    if (!debug_mode && !daemon_user_defined(&config.user))
        arg_error(argv[0], "no user name specified (-u)");
}
