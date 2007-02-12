#include "workshop.h"
#include "version.h"

#include <daemon/daemonize.h>

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>

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
         "\n"
         );
}

/** read configuration options from the command line */
void parse_cmdline(struct config *config, int argc, char **argv) {
    int ret;
#ifdef __GLIBC__
    static const struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"version", 0, 0, 'V'},
        {"verbose", 0, 0, 'v'},
        {"quiet", 1, 0, 'q'},
        {"name", 1, 0, 'N'},
        {"concurrency", 1, 0, 'c'},
        {"database", 1, 0, 'd'},
        {"logger", 1, 0, 'l'},
        {"pidfile", 1, 0, 'P'},
        {0,0,0,0}
    };
#endif

    config->database = getenv("WORKSHOP_DATABASE");
    daemon_config.logger = getenv("WORKSHOP_LOGGER");

    while (1) {
#ifdef __GLIBC__
        int option_index = 0;

        ret = getopt_long(argc, argv, "hVvqp:DP:l:r:u:",
                          long_options, &option_index);
#else
        ret = getopt(argc, argv, "hVvqp:DP:l:r:u:");
#endif
        if (ret == -1)
            break;

        switch (ret) {
        case 'h':
            usage();
            exit(0);

        case 'V':
            printf("uoproxy v" VERSION
                   ", http://max.kellermann.name/projects/uoproxy/\n");
            exit(0);

        case 'v':
            ++daemon_verbose;
            break;

        case 'q':
            daemon_verbose = 0;
            break;

        case 'N':
            config->node_name = optarg;
            break;

        case 'c':
            config->concurrency = (unsigned)strtoul(optarg, NULL, 10);
            if (config->concurrency == 0) {
                fprintf(stderr, "invalid concurrency specification\n");
                exit(1);
            }
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

        default:
            exit(1);
        }
    }

    /* check non-option arguments */

    if (optind < argc) {
        fprintf(stderr, "cm4all-workshop: unrecognized argument: %s\n",
                argv[optind]);
        fprintf(stderr, "Try 'cm4all-workshop -h' for more information\n");
        exit(1);
    }

    /* check completeness */

    if (config->node_name == NULL) {
        fprintf(stderr, "cm4all-workshop: no node name specified\n");
        exit(1);
    }

    if (config->database == NULL) {
        fprintf(stderr, "cm4all-workshop: no database specified\n");
        exit(1);
    }
}

void config_dispose(struct config *config) {
    (void)config;
}
