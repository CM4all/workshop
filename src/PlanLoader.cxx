/*
 * Parses plan configuration files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Plan.hxx"
#include "util/CharUtil.hxx"
#include "util/StringUtil.hxx"
#include "util/Tokenizer.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "io/TextFile.hxx"

#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

static constexpr Domain plan_loader_domain("plan_loader");

gcc_pure
static bool
user_in_group(const struct group *group, const char *user)
{
    char **mem = group->gr_mem;

    while (*mem != nullptr) {
        if (strcmp(*mem, user) == 0)
            return true;
        ++mem;
    }

    return false;
}

static std::vector<gid_t>
get_user_groups(const char *user)
{
    std::vector<gid_t> groups;

    setgrent();

    const struct group *group;
    while ((group = getgrent()) != nullptr) {
        if (group->gr_gid > 0 && user_in_group(group, user))
            groups.push_back(group->gr_gid);
    }

    endgrent();

    return groups;
}

inline bool
Plan::ParseLine(Tokenizer &tokenizer, Error &error)
{
    if (tokenizer.IsEnd() || tokenizer.CurrentChar() == '#')
        return true;

    const char *key = tokenizer.NextWord(error);
    if (key == nullptr) {
        assert(error.IsDefined());
        return false;
    }

    const char *value = tokenizer.NextParam(error);
    if (value == nullptr) {
        if (!error.IsDefined())
            error.Set(plan_loader_domain, "value missing after keyword");
        return false;
    }

    if (strcmp(key, "exec") == 0) {
        if (!args.empty()) {
            error.Set(plan_loader_domain, "'exec' already specified");
            return false;
        }

        if (*value == 0) {
            error.Set(plan_loader_domain, "empty executable");
            return false;
        }

        while (value != nullptr) {
            args.push_back(value);
            value = tokenizer.NextParam(error);
        }

        if (error.IsDefined())
            return false;

        return true;
    }

    if (!tokenizer.IsEnd()) {
        error.Set(plan_loader_domain, "too many arguments");
        return false;
    }

    if (strcmp(key, "timeout") == 0) {
        timeout = value;
    } else if (strcmp(key, "chroot") == 0) {
        int ret;
        struct stat st;

        ret = stat(value, &st);
        if (ret < 0) {
            error.FormatErrno("failed to stat '%s'", value);
            return false;
        }

        if (!S_ISDIR(st.st_mode)) {
            error.Format(plan_loader_domain, "not a directory: %s", value);
            return false;
        }

        chroot = value;
    } else if (strcmp(key, "user") == 0) {
        struct passwd *pw;

        pw = getpwnam(value);
        if (pw == nullptr) {
            error.Format(plan_loader_domain, "no such user '%s'", value);
            return false;
        }

        if (pw->pw_uid == 0) {
            error.Set(plan_loader_domain, "user 'root' is forbidden");
            return false;
        }

        if (pw->pw_gid == 0) {
            error.Set(plan_loader_domain, "group 'root' is forbidden");
            return false;
        }

        uid = pw->pw_uid;
        gid = pw->pw_gid;

        groups = get_user_groups(value);
    } else if (strcmp(key, "nice") == 0) {
        priority = atoi(value);
    } else if (strcmp(key, "concurrency") == 0) {
        concurrency = (unsigned)strtoul(value, nullptr, 0);
    } else {
        error.Format(plan_loader_domain, "unknown option '%s'", key);
        return false;
    }

    return true;
}

inline bool
Plan::LoadFile(TextFile &file, Error &error)
{
    char *line;
    while ((line = file.ReadLine()) != nullptr) {
        Tokenizer tokenizer(line);
        if (!ParseLine(tokenizer, error)) {
            file.PrefixError(error);
            return false;
        }
    }

    if (args.empty()) {
        error.Format(plan_loader_domain, "no 'exec' in %s", file.GetPath());
        return false;
    }

    if (timeout.empty())
        timeout = "10 minutes";

    return true;
}

bool
Plan::LoadFile(const char *path, Error &error)
{
    assert(path != nullptr);

    TextFile *file = TextFile::Open(path, error);
    if (file == nullptr)
        return false;

    const bool success = LoadFile(*file, error);
    delete file;
    return success;
}
