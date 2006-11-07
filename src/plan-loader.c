/*
 * $Id$
 *
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
        while (**pp < 0 || **pp > 0x20)
            ++(*pp);
    }

    if (**pp == 0)
        return word;

    **pp = 0;
    ++(*pp);

    return word;
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
            } else if (strcmp(key, "nice") == 0) {
                plan->priority = atoi(value);
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

    free(plan);
}
