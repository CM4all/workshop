/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "PlanLoader.hxx"
#include "Plan.hxx"
#include "system/Error.hxx"
#include "io/TextFile.hxx"
#include "util/Tokenizer.hxx"
#include "util/RuntimeError.hxx"

#include <inline/compiler.h>

#include <array>

#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

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

static void
ParseLine(Plan &plan, Tokenizer &tokenizer)
{
    if (tokenizer.IsEnd() || tokenizer.CurrentChar() == '#')
        return;

    const char *key = tokenizer.NextWord();
    assert(key != nullptr);

    const char *value = tokenizer.NextParam();
    if (value == nullptr)
        throw std::runtime_error("value missing after keyword");

    if (strcmp(key, "exec") == 0) {
        if (!plan.args.empty())
            throw std::runtime_error("'exec' already specified");

        if (*value == 0)
            throw std::runtime_error("empty executable");

        while (value != nullptr) {
            plan.args.push_back(value);
            value = tokenizer.NextParam();
        }

        return;
    }

    if (!tokenizer.IsEnd())
        throw std::runtime_error("too many arguments");

    if (strcmp(key, "timeout") == 0) {
        plan.timeout = value;
    } else if (strcmp(key, "chroot") == 0) {
        int ret;
        struct stat st;

        ret = stat(value, &st);
        if (ret < 0)
            throw FormatErrno("failed to stat '%s'", value);

        if (!S_ISDIR(st.st_mode))
            throw FormatRuntimeError("not a directory: %s", value);

        plan.chroot = value;
    } else if (strcmp(key, "user") == 0) {
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
        plan.priority = atoi(value);
    } else if (strcmp(key, "concurrency") == 0) {
        plan.concurrency = (unsigned)strtoul(value, nullptr, 0);
    } else
        throw FormatRuntimeError("unknown option '%s'", key);
}

static Plan
LoadPlanFile(TextFile &file)
{
    Plan plan;

    char *line;
    while ((line = file.ReadLine()) != nullptr) {
        try {
            Tokenizer tokenizer(line);
            ParseLine(plan, tokenizer);
        } catch (const std::runtime_error &e) {
            std::throw_with_nested(FormatRuntimeError("%s line %u",
                                                      file.GetPath(),
                                                      file.GetLineNumber()));
        }
    }

    if (plan.args.empty())
        throw FormatRuntimeError("no 'exec' in %s", file.GetPath());

    if (plan.timeout.empty())
        plan.timeout = "10 minutes";

    return plan;
}

Plan
LoadPlanFile(const char *path)
{
    assert(path != nullptr);

    TextFile file(path);
    return LoadPlanFile(file);
}
