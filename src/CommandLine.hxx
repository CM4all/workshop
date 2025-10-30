// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

struct Config;

/** read configuration options from the command line */
void
ParseCommandLine(int argc, char **argv);
