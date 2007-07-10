/*
 * Parses plan configuration files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "workshop.h"
#include "plan-internal.h"

#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

/** parse the next word from the writable string */
static char *next_word(char **pp) {
    char *word;

    while (**pp > 0 && **pp <= 0x20)
        ++(*pp);

    if (**pp == 0)
        return NULL;

    if (**pp == '"') {
        word = ++(*pp);
        while (**pp != 0 && **pp != '"')
            ++(*pp);
    } else {
        word = *pp;
        while (((unsigned char)**pp) > 0x20)
            ++(*pp);
    }

    if (**pp == 0)
        return word;

    **pp = 0;
    ++(*pp);

    return word;
}

static int user_in_group(const struct group *group, const char *user) {
    char **mem = group->gr_mem;

    while (*mem != NULL) {
        if (strcmp(*mem, user) == 0)
            return 1;
        ++mem;
    }

    return 0;
}

static int get_user_groups(const char *user, gid_t **groups_r) {
    unsigned num_groups = 0, max_groups = 0;
    gid_t *groups = NULL, *groups_new;
    const struct group *group;

    setgrent();

    while ((group = getgrent()) != NULL) {
        if (group->gr_gid > 0 && user_in_group(group, user)) {
            if (num_groups >= max_groups) {
                max_groups += 16;
                groups_new = realloc(groups, max_groups * sizeof(*groups));
                if (groups_new == NULL) {
                    free(groups);
                    endgrent();
                    fprintf(stderr, "Out of memory\n");
                    return -1;
                }

                groups = groups_new;
            }

            groups[num_groups++] = group->gr_gid;
        }
    }

    endgrent();

    *groups_r = groups;
    return (int)num_groups;
}

static int parse_plan_config(struct plan *plan, FILE *file) {
    char line[1024], *p, *key, *value;
    unsigned line_no = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_no;

        p = line;
        key = next_word(&p);
        if (key == NULL || *key == '#')
            continue;

        value = next_word(&p);
        if (value == NULL) {
            fprintf(stderr, "line %u: value missing after keyword\n",
                    line_no);
            return -1;
        }

        if (strcmp(key, "exec") == 0) {
            if (plan->argv.num > 0) {
                fprintf(stderr, "line %u: 'exec' already specified\n",
                        line_no);
                return -1;
            }

            if (*value == 0) {
                fprintf(stderr, "line %u: empty executable\n",
                        line_no);
                return -1;
            }

            while (value != NULL) {
                strarray_append(&plan->argv, value);
                value = next_word(&p);
            }
        } else {
            p = next_word(&p);
            if (p != NULL) {
                fprintf(stderr, "line %u: too many arguments\n",
                        line_no);
                return -1;
            }

            if (strcmp(key, "timeout") == 0) {
                plan->timeout = strdup(value);
                if (plan->timeout == NULL)
                    return errno;
            } else if (strcmp(key, "chroot") == 0) {
                int ret;
                struct stat st;

                ret = stat(value, &st);
                if (ret < 0) {
                    fprintf(stderr, "line %u: failed to stat '%s': %s\n",
                            line_no, value, strerror(errno));
                    return -1;
                }

                if (!S_ISDIR(st.st_mode)) {
                    fprintf(stderr, "line %u: not a directory: %s\n",
                            line_no, value);
                    return -1;
                }

                plan->chroot = strdup(value);
                if (plan->chroot == NULL)
                    return errno;
            } else if (strcmp(key, "user") == 0) {
                struct passwd *pw;

                pw = getpwnam(value);
                if (pw == NULL) {
                    fprintf(stderr, "line %u: no such user '%s'\n",
                            line_no, value);
                    return -1;
                }

                if (pw->pw_uid == 0) {
                    fprintf(stderr, "user 'root' is forbidden\n");
                    return -1;
                }

                if (pw->pw_gid == 0) {
                    fprintf(stderr, "group 'root' is forbidden\n");
                    return -1;
                }

                plan->uid = pw->pw_uid;
                plan->gid = pw->pw_gid;

                plan->num_groups = get_user_groups(value, &plan->groups);
                if (plan->num_groups < 0)
                    return -1;
            } else if (strcmp(key, "nice") == 0) {
                plan->priority = atoi(value);
            } else if (strcmp(key, "concurrency") == 0) {
                plan->concurrency = (unsigned)strtoul(value, NULL, 0);
            } else {
                fprintf(stderr, "line %u: unknown option '%s'\n",
                        line_no, key);
                return -1;
            }
        }
    }

    if (plan->argv.num == 0) {
        fprintf(stderr, "no 'exec'\n");
        return -1;
    }

    if (plan->timeout == NULL) {
        plan->timeout = strdup("10 minutes");
        if (plan->timeout == NULL)
            return errno;
    }

    return 0;
}

int plan_load(const char *path, struct plan **plan_r) {
    struct plan *plan;
    FILE *file;
    int ret;

    assert(path != NULL);

    plan = calloc(1, sizeof(*plan));
    if (plan == NULL)
        return ENOMEM;

    plan->uid = 65534;
    plan->gid = 65534;
    plan->priority = 10;

    file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "failed to open file '%s': %s\n",
                path, strerror(errno));
        plan_free(&plan);
        return -1;
    }

    ret = parse_plan_config(plan, file);
    fclose(file);
    if (ret != 0) {
        fprintf(stderr, "parsing file '%s' failed\n", path);
        plan_free(&plan);
        return ret;
    }

    *plan_r = plan;
    return 0;
}

void plan_free(struct plan **plan_r) {
    struct plan *plan;

    assert(plan_r != NULL);
    assert(*plan_r != NULL);

    plan = *plan_r;
    *plan_r = NULL;

    assert(plan->ref == 0);

    strarray_free(&plan->argv);

    if (plan->timeout != NULL)
        free(plan->timeout);

    if (plan->chroot != NULL)
        free(plan->chroot);

    if (plan->groups != NULL)
        free(plan->groups);

    free(plan);
}
