/*
 * Parses plan configuration files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Plan.hxx"
#include "util/CharUtil.hxx"
#include "util/StringUtil.hxx"

#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

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
parse_plan_config(Plan &plan, FILE *file)
{
    char line[1024], *p, *key, *value;
    unsigned line_no = 0;

    while (fgets(line, sizeof(line), file) != nullptr) {
        ++line_no;

        p = line;
        key = NextWord(p);
        if (key == nullptr || *key == '#')
            continue;

        value = NextWord(p);
        if (value == nullptr) {
            fprintf(stderr, "line %u: value missing after keyword\n",
                    line_no);
            return false;
        }

        if (strcmp(key, "exec") == 0) {
            if (!plan.args.empty()) {
                fprintf(stderr, "line %u: 'exec' already specified\n",
                        line_no);
                return false;
            }

            if (*value == 0) {
                fprintf(stderr, "line %u: empty executable\n",
                        line_no);
                return false;
            }

            while (value != nullptr) {
                plan.args.push_back(value);
                value = NextWord(p);
            }
        } else {
            p = NextWord(p);
            if (p != nullptr) {
                fprintf(stderr, "line %u: too many arguments\n",
                        line_no);
                return false;
            }

            if (strcmp(key, "timeout") == 0) {
                plan.timeout = value;
            } else if (strcmp(key, "chroot") == 0) {
                int ret;
                struct stat st;

                ret = stat(value, &st);
                if (ret < 0) {
                    fprintf(stderr, "line %u: failed to stat '%s': %s\n",
                            line_no, value, strerror(errno));
                    return false;
                }

                if (!S_ISDIR(st.st_mode)) {
                    fprintf(stderr, "line %u: not a directory: %s\n",
                            line_no, value);
                    return false;
                }

                plan.chroot = value;
            } else if (strcmp(key, "user") == 0) {
                struct passwd *pw;

                pw = getpwnam(value);
                if (pw == nullptr) {
                    fprintf(stderr, "line %u: no such user '%s'\n",
                            line_no, value);
                    return false;
                }

                if (pw->pw_uid == 0) {
                    fprintf(stderr, "user 'root' is forbidden\n");
                    return false;
                }

                if (pw->pw_gid == 0) {
                    fprintf(stderr, "group 'root' is forbidden\n");
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
                fprintf(stderr, "line %u: unknown option '%s'\n",
                        line_no, key);
                return false;
            }
        }
    }

    if (plan.args.empty()) {
        fprintf(stderr, "no 'exec'\n");
        return false;
    }

    if (plan.timeout.empty())
        plan.timeout = "10 minutes";

    return true;
}

bool
Plan::LoadFile(const char *path)
{
    FILE *file;

    assert(path != nullptr);

    file = fopen(path, "r");
    if (file == nullptr) {
        fprintf(stderr, "failed to open file '%s': %s\n",
                path, strerror(errno));
        return false;
    }

    const bool success = parse_plan_config(*this, file);
    fclose(file);
    if (!success) {
        fprintf(stderr, "parsing file '%s' failed\n", path);
        return false;
    }

    return true;
}
