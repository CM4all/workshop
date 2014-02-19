/*
 * Parses plan configuration files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Plan.hxx"
#include "util/CharUtil.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

static constexpr Domain plan_loader_domain("plan_loader");

/** parse the next word from the writable string */
static char *
NextWord(char *&p)
{
    char *word;

    p = StripLeft(p);

    if (*p == 0)
        return nullptr;

    if (*p == '"') {
        word = ++p;
        while (*p != 0 && *p != '"')
            ++p;
    } else {
        word = p;
        while (!IsWhitespaceOrNull(*p))
            ++p;
    }

    if (*p == 0)
        return word;

    *p = 0;
    ++p;

    return word;
}

static int user_in_group(const struct group *group, const char *user) {
    char **mem = group->gr_mem;

    while (*mem != nullptr) {
        if (strcmp(*mem, user) == 0)
            return 1;
        ++mem;
    }

    return 0;
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

static bool
parse_plan_line(Plan &plan, char *line, Error &error)
{
    char *p = line;
    const char *key = NextWord(p);
    if (key == nullptr || *key == '#')
        return true;

    const char *value = NextWord(p);
    if (value == nullptr) {
        error.Set(plan_loader_domain, "value missing after keyword");
        return false;
    }

    if (strcmp(key, "exec") == 0) {
        if (!plan.args.empty()) {
            error.Set(plan_loader_domain, "'exec' already specified");
            return false;
        }

        if (*value == 0) {
            error.Set(plan_loader_domain, "empty executable");
            return false;
        }

        while (value != nullptr) {
            plan.args.push_back(value);
            value = NextWord(p);
        }

        return true;
    }

    p = NextWord(p);
    if (p != nullptr) {
        error.Set(plan_loader_domain, "too many arguments");
        return false;
    }

    if (strcmp(key, "timeout") == 0) {
        plan.timeout = value;
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

        plan.chroot = value;
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

        plan.uid = pw->pw_uid;
        plan.gid = pw->pw_gid;

        plan.groups = get_user_groups(value);
    } else if (strcmp(key, "nice") == 0) {
        plan.priority = atoi(value);
    } else if (strcmp(key, "concurrency") == 0) {
        plan.concurrency = (unsigned)strtoul(value, nullptr, 0);
    } else {
        error.Format(plan_loader_domain, "unknown option '%s'", key);
        return false;
    }

    return true;
}

static bool
parse_plan_config(Plan &plan, const char *path, FILE *file, Error &error)
{
    char line[1024];
    unsigned line_no = 0;

    while (fgets(line, sizeof(line), file) != nullptr) {
        ++line_no;

        if (!parse_plan_line(plan, line, error)) {
            error.FormatPrefix("in %s line %u: ", path, line_no);
            return false;
        }
    }

    if (plan.args.empty()) {
        error.Format(plan_loader_domain, "no 'exec' in %s", path);
        return false;
    }

    if (plan.timeout.empty())
        plan.timeout = "10 minutes";

    return true;
}

bool
Plan::LoadFile(const char *path, Error &error)
{
    FILE *file;

    assert(path != nullptr);

    file = fopen(path, "r");
    if (file == nullptr) {
        error.FormatErrno("Failed to open file %s", path);
        return false;
    }

    const bool success = parse_plan_config(*this, path, file, error);
    fclose(file);
    return success;
}
