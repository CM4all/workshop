/*
 * Copyright 2006-2018 Content Management AG
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

#include "PlanLoader.hxx"
#include "Plan.hxx"
#include "pg/Interval.hxx"
#include "system/Error.hxx"
#include "io/FileLineParser.hxx"
#include "io/ConfigParser.hxx"
#include "util/RuntimeError.hxx"

#include "util/Compiler.h"

#include <array>

#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

class PlanLoader final : public ConfigParser {
	Plan plan;

public:
	Plan &&Release() {
		return std::move(plan);
	}

	/* virtual methods from class ConfigParser */
	void ParseLine(FileLineParser &line) final;
	void Finish() override;
};

gcc_pure
static std::vector<gid_t>
get_user_groups(const char *user, gid_t gid)
{
	std::array<gid_t, 64> groups;
	int ngroups = groups.size();
	int result = getgrouplist(user, gid,
				  &groups.front(), &ngroups);
	if (result < 0)
		throw std::runtime_error("getgrouplist() failed");

	return std::vector<gid_t>(groups.begin(),
				  std::next(groups.begin(), result));
}

void
PlanLoader::ParseLine(FileLineParser &line)
{
	const char *key = line.ExpectWord();

	if (strcmp(key, "exec") == 0) {
		if (!plan.args.empty())
			throw std::runtime_error("'exec' already specified");

		const char *value = line.NextRelaxedValue();
		if (value == nullptr || *value == 0)
			throw std::runtime_error("empty executable");

		do {
			plan.args.emplace_back(value);
			value = line.NextRelaxedValue();
		} while (value != nullptr);
	} else if (strcmp(key, "control_channel") == 0) {
		/* previously, the "yes"/"no" parameter was mandatory, but
		   that's deprecated since 2.0.36 */
		plan.control_channel = line.IsEnd() || line.NextBool();
		line.ExpectEnd();
	} else if (strcmp(key, "timeout") == 0) {
		plan.timeout = line.ExpectValueAndEnd();
		plan.parsed_timeout = Pg::ParseIntervalS(plan.timeout.c_str());
	} else if (strcmp(key, "reap_finished") == 0) {
		plan.reap_finished = line.ExpectValueAndEnd();
		auto d = Pg::ParseIntervalS(plan.reap_finished.c_str());
		if (d.count() <= 0)
			throw FormatRuntimeError("Not a positive duration: %s",
						 plan.reap_finished.c_str());
	} else if (strcmp(key, "chroot") == 0) {
		const char *value = line.ExpectValueAndEnd();

		int ret;
		struct stat st;

		ret = stat(value, &st);
		if (ret < 0)
			throw FormatErrno("failed to stat '%s'", value);

		if (!S_ISDIR(st.st_mode))
			throw FormatRuntimeError("not a directory: %s", value);

		plan.chroot = value;
	} else if (strcmp(key, "user") == 0) {
		const char *value = line.ExpectValueAndEnd();

		struct passwd *pw;

		pw = getpwnam(value);
		if (pw == nullptr)
			throw FormatRuntimeError("no such user '%s'", value);

		if (pw->pw_uid == 0)
			throw std::runtime_error("user 'root' is forbidden");

		if (pw->pw_gid == 0)
			throw std::runtime_error("group 'root' is forbidden");

		plan.uid = pw->pw_uid;
		plan.gid = pw->pw_gid;

		plan.groups = get_user_groups(value, plan.gid);
	} else if (strcmp(key, "umask") == 0) {
		const char *s = line.ExpectValueAndEnd();
		if (*s != '0')
			throw std::runtime_error("umask must be an octal value starting with '0'");

		char *endptr;
		auto value = strtoul(s, &endptr, 8);
		if (endptr == s || *endptr != 0)
			throw std::runtime_error("Failed to parse umask");

		if (value & ~0777)
			throw std::runtime_error("umask is too large");

		plan.umask = value;
	} else if (strcmp(key, "nice") == 0) {
		plan.priority = atoi(line.ExpectValueAndEnd());
	} else if (strcmp(key, "sched_idle") == 0) {
		plan.sched_idle = true;
		line.ExpectEnd();
	} else if (strcmp(key, "ioprio_idle") == 0) {
		plan.ioprio_idle = true;
		line.ExpectEnd();
	} else if (strcmp(key, "idle") == 0) {
		plan.sched_idle = plan.ioprio_idle = true;
		line.ExpectEnd();
	} else if (strcmp(key, "private_network") == 0) {
		line.ExpectEnd();
		plan.private_network = true;
	} else if (strcmp(key, "rlimits") == 0) {
		if (!plan.rlimits.Parse(line.ExpectValueAndEnd()))
			throw std::runtime_error("Failed to parse rlimits");
	} else if (strcmp(key, "concurrency") == 0) {
		plan.concurrency = line.NextPositiveInteger();
		line.ExpectEnd();
	} else if (strcmp(key, "rate_limit") == 0) {
		plan.rate_limits.emplace_back(RateLimit::Parse(line.ExpectValueAndEnd()));
	} else
		throw FormatRuntimeError("unknown option '%s'", key);
}

void
PlanLoader::Finish()
{
	if (plan.args.empty())
		throw std::runtime_error("no 'exec'");

	if (plan.timeout.empty())
		plan.timeout = "10 minutes";
}

Plan
LoadPlanFile(const std::filesystem::path &path)
{
	PlanLoader loader;
	CommentConfigParser comment_parser(loader);
	ParseConfigFile(path, comment_parser);
	return loader.Release();
}
