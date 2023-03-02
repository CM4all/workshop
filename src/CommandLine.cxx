// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CommandLine.hxx"
#include "Config.hxx"
#include "debug.h"
#include "version.h"
#include "io/Logger.hxx"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
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
	exit(EXIT_FAILURE);
}

/** read configuration options from the command line */
void
ParseCommandLine(int argc, char **argv)
{
	int ret;
#ifdef __GLIBC__
	static const struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'V'},
		{"verbose", 0, 0, 'v'},
		{"quiet", 0, 0, 'q'},
		{0,0,0,0}
	};
#endif

	unsigned log_level = 1;

	while (1) {
#ifdef __GLIBC__
		int option_index = 0;

		ret = getopt_long(argc, argv, "hVvq",
				  long_options, &option_index);
#else
		ret = getopt(argc, argv, "hVvq");
#endif
		if (ret == -1)
			break;

		switch (ret) {
		case 'h':
			usage();
			exit(EXIT_SUCCESS);

		case 'V':
			printf(PACKAGE " v%s\n", VERSION);
			exit(EXIT_SUCCESS);

		case 'v':
			++log_level;
			break;

		case 'q':
			log_level = 0;
			break;

		case '?':
			arg_error(argv[0], nullptr);

		default:
			exit(EXIT_FAILURE);
		}
	}

	SetLogLevel(log_level);

	/* check non-option arguments */

	if (optind < argc)
		arg_error(argv[0], "unrecognized argument: %s", argv[optind]);
}
