/*
 * Parses plan configuration files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Plan.hxx"
#include "system/Error.hxx"
#include "io/TextFile.hxx"
#include "util/Tokenizer.hxx"
#include "util/RuntimeError.hxx"

#include <inline/compiler.h>

#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

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

inline void
Plan::ParseLine(Tokenizer &tokenizer)
{
    if (tokenizer.IsEnd() || tokenizer.CurrentChar() == '#')
        return;

    const char *key = tokenizer.NextWord();
    assert(key != nullptr);

    const char *value = tokenizer.NextParam();
    if (value == nullptr)
        throw std::runtime_error("value missing after keyword");

    if (strcmp(key, "exec") == 0) {
        if (!args.empty())
            throw std::runtime_error("'exec' already specified");

        if (*value == 0)
            throw std::runtime_error("empty executable");

        while (value != nullptr) {
            args.push_back(value);
            value = tokenizer.NextParam();
        }

        return;
    }

    if (!tokenizer.IsEnd())
        throw std::runtime_error("too many arguments");

    if (strcmp(key, "timeout") == 0) {
        timeout = value;
    } else if (strcmp(key, "chroot") == 0) {
        int ret;
        struct stat st;

        ret = stat(value, &st);
        if (ret < 0)
            throw FormatErrno("failed to stat '%s'", value);

        if (!S_ISDIR(st.st_mode))
            throw FormatRuntimeError("not a directory: %s", value);

        chroot = value;
    } else if (strcmp(key, "user") == 0) {
        struct passwd *pw;

        pw = getpwnam(value);
        if (pw == nullptr)
            throw FormatRuntimeError("no such user '%s'", value);

        if (pw->pw_uid == 0)
            throw std::runtime_error("user 'root' is forbidden");

        if (pw->pw_gid == 0)
            throw std::runtime_error("group 'root' is forbidden");

        uid = pw->pw_uid;
        gid = pw->pw_gid;

        groups = get_user_groups(value);
    } else if (strcmp(key, "nice") == 0) {
        priority = atoi(value);
    } else if (strcmp(key, "concurrency") == 0) {
        concurrency = (unsigned)strtoul(value, nullptr, 0);
    } else
        throw FormatRuntimeError("unknown option '%s'", key);
}

inline void
Plan::LoadFile(TextFile &file)
{
    char *line;
    while ((line = file.ReadLine()) != nullptr) {
        try {
            Tokenizer tokenizer(line);
            ParseLine(tokenizer);
        } catch (const std::runtime_error &e) {
            std::throw_with_nested(FormatRuntimeError("%s line %u",
                                                      file.GetPath(),
                                                      file.GetLineNumber()));
        }
    }

    if (args.empty())
        throw FormatRuntimeError("no 'exec' in %s", file.GetPath());

    if (timeout.empty())
        timeout = "10 minutes";
}

void
Plan::LoadFile(const char *path)
{
    assert(path != nullptr);

    TextFile file(path);
    LoadFile(file);
}
