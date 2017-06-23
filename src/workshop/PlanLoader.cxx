/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "PlanLoader.hxx"
#include "Plan.hxx"
#include "system/Error.hxx"
#include "io/FileLineParser.hxx"
#include "io/ConfigParser.hxx"
#include "util/RuntimeError.hxx"

#include <inline/compiler.h>

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
            plan.args.push_back(value);
            value = line.NextRelaxedValue();
        } while (value != nullptr);
    } else if (strcmp(key, "timeout") == 0) {
        plan.timeout = line.ExpectValueAndEnd();
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
    } else if (strcmp(key, "nice") == 0) {
        plan.priority = atoi(line.ExpectValueAndEnd());
    } else if (strcmp(key, "concurrency") == 0) {
        plan.concurrency = line.NextPositiveInteger();
        line.ExpectEnd();
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
LoadPlanFile(const boost::filesystem::path &path)
{
    PlanLoader loader;
    CommentConfigParser comment_parser(loader);
    ParseConfigFile(path, comment_parser);
    return loader.Release();
}
