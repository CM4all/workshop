// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "PlanLoader.hxx"
#include "Plan.hxx"
#include "pg/Interval.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/config/FileLineParser.hxx"
#include "io/config/ConfigParser.hxx"
#include "util/StringAPI.hxx"

#include <array>

#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

class PlanLoader final : public ConfigParser {
	Plan plan;

	bool seen_exec_option = false;

public:
	Plan &&Release() {
		return std::move(plan);
	}

	/* virtual methods from class ConfigParser */
	void ParseLine(FileLineParser &line) final;
	void Finish() override;
};

[[gnu::pure]]
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

	if (StringIsEqual(key, "exec")) {
		if (!plan.args.empty())
			throw std::runtime_error("'exec' already specified");

		const char *value = line.NextRelaxedValue();
		if (value == nullptr || *value == 0)
			throw std::runtime_error("empty executable");

		do {
			plan.args.emplace_back(value);
			value = line.NextRelaxedValue();
		} while (value != nullptr);

		seen_exec_option = true;
	} else if (StringIsEqual(key, "translate")) {
		plan.translate = true;
	} else if (StringIsEqual(key, "control_channel")) {
		/* previously, the "yes"/"no" parameter was mandatory, but
		   that's deprecated since 2.0.36 */
		plan.control_channel = line.IsEnd() || line.NextBool();
		line.ExpectEnd();
	} else if (StringIsEqual(key, "allow_spawn")) {
		if (!plan.control_channel)
			throw std::runtime_error{"allow_spawn requires control_channel"};

		plan.allow_spawn = true;
		line.ExpectEnd();
	} else if (StringIsEqual(key, "timeout")) {
		plan.timeout = line.ExpectValueAndEnd();
		plan.parsed_timeout = Pg::ParseIntervalS(plan.timeout.c_str());
	} else if (StringIsEqual(key, "reap_finished")) {
		plan.reap_finished = line.ExpectValueAndEnd();
		auto d = Pg::ParseIntervalS(plan.reap_finished.c_str());
		if (d.count() <= 0)
			throw FmtRuntimeError("Not a positive duration: {}",
					      plan.reap_finished);
	} else if (StringIsEqual(key, "chroot")) {
		const char *value = line.ExpectValueAndEnd();

		int ret;
		struct stat st;

		ret = stat(value, &st);
		if (ret < 0)
			throw FmtErrno("failed to stat {:?}", value);

		if (!S_ISDIR(st.st_mode))
			throw FmtRuntimeError("not a directory: {}", value);

		plan.chroot = value;
		seen_exec_option = true;
	} else if (StringIsEqual(key, "user")) {
		const char *value = line.ExpectValueAndEnd();

		struct passwd *pw;

		pw = getpwnam(value);
		if (pw == nullptr)
			throw FmtRuntimeError("no such user {:?}", value);

		if (pw->pw_uid == 0)
			throw std::runtime_error("user 'root' is forbidden");

		if (pw->pw_gid == 0)
			throw std::runtime_error("group 'root' is forbidden");

		plan.uid = pw->pw_uid;
		plan.gid = pw->pw_gid;

		plan.groups = get_user_groups(value, plan.gid);

		seen_exec_option = true;
	} else if (StringIsEqual(key, "umask")) {
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
		seen_exec_option = true;
	} else if (StringIsEqual(key, "nice")) {
		plan.priority = atoi(line.ExpectValueAndEnd());
		seen_exec_option = true;
	} else if (StringIsEqual(key, "sched_idle")) {
		plan.sched_idle = true;
		line.ExpectEnd();
		seen_exec_option = true;
	} else if (StringIsEqual(key, "ioprio_idle")) {
		plan.ioprio_idle = true;
		line.ExpectEnd();
		seen_exec_option = true;
	} else if (StringIsEqual(key, "idle")) {
		plan.sched_idle = plan.ioprio_idle = true;
		line.ExpectEnd();
		seen_exec_option = true;
	} else if (StringIsEqual(key, "private_network")) {
		line.ExpectEnd();
		plan.private_network = true;
		seen_exec_option = true;
	} else if (StringIsEqual(key, "private_tmp")) {
		line.ExpectEnd();
		plan.private_tmp = true;
		seen_exec_option = true;
	} else if (StringIsEqual(key, "rlimits")) {
		if (!plan.rlimits.Parse(line.ExpectValueAndEnd()))
			throw std::runtime_error("Failed to parse rlimits");
		seen_exec_option = true;
	} else if (StringIsEqual(key, "concurrency")) {
		plan.concurrency = line.NextPositiveInteger();
		line.ExpectEnd();
	} else if (StringIsEqual(key, "rate_limit")) {
		plan.rate_limits.emplace_back(RateLimit::Parse(line.ExpectValueAndEnd()));
	} else
		throw FmtRuntimeError("unknown option {:?}", key);
}

void
PlanLoader::Finish()
{
	if (plan.translate) {
		if (seen_exec_option)
			throw std::runtime_error("Cannot use 'translate' with execute options");

		if (plan.allow_spawn)
			/* this is forbidden until we have a secure
			   implementation */
			throw std::runtime_error("Cannot use 'translate' with 'allow_spawn'");
	} else {
		if (plan.args.empty())
			throw std::runtime_error("no 'exec'");
	}

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
