/*
 * Copyright 2006-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
#ifdef __GLIBC__
	     " --name NAME\n"
#endif
	     " -N NAME        set the node name\n"
#ifdef __GLIBC__
	     " --concurrency NUM\n"
#endif
	     " -c NUM         set the maximum number of concurrent operators (default: 2)\n"
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
	exit(EXIT_FAILURE);
}

/** read configuration options from the command line */
void
ParseCommandLine(Config &config, int argc, char **argv)
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
		{"user", 1, 0, 'u'},
		{0,0,0,0}
	};
#endif

	unsigned log_level = 1;

	while (1) {
#ifdef __GLIBC__
		int option_index = 0;

		ret = getopt_long(argc, argv, "hVvqN:c:u:",
				  long_options, &option_index);
#else
		ret = getopt(argc, argv, "hVvqN:c:u:");
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

		case 'N':
			config.node_name = optarg;
			break;

		case 'c':
			config.concurrency = (unsigned)strtoul(optarg, nullptr, 10);
			if (config.concurrency == 0)
				arg_error(argv[0], "invalid concurrency specification");
			break;

		case 'u':
			if (debug_mode)
				arg_error(argv[0], "cannot specify a user in debug mode");

			config.user.Lookup(optarg);
			if (!config.user.IsComplete())
				arg_error(argv[0], "refusing to run as root");
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
