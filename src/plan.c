/*
 * $Id$
 *
 * Manage the list of plans (= library) and load their configuration
 * files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "workshop.h"

#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>

struct plan_entry {
    struct plan *plan;
    time_t mtime;
};

struct library {
    char *path;
    struct plan_entry *plans;
    unsigned num_plans, max_plans;
    unsigned ref;
};

static void free_plan(struct plan **plan_r) {
    struct plan *plan;

    assert(plan_r != NULL);
    assert(*plan_r != NULL);

    plan = *plan_r;
    *plan_r = NULL;

    assert(plan->ref == 0);

    if (plan->name != NULL)
        free(plan->name);

    if (plan->argv != NULL) {
        unsigned i;

        for (i = 0; i < plan->argc; ++i)
            if (plan->argv[i] != NULL)
                free(plan->argv[i]);

        free(plan->argv);
    }

    if (plan->timeout != NULL)
        free(plan->timeout);

    if (plan->chroot != NULL)
        free(plan->chroot);

    free(plan);
}

int library_open(const char *path, struct library **library_r) {
    int ret;
    struct stat st;
    struct library *library;

    assert(path != NULL);

    /* check path */

    ret = stat(path, &st);
    if (ret < 0) {
        fprintf(stderr, "failed to stat '%s': %s\n",
                path, strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "not a directory: %s\n", path);
        return -1;
    }

    /* create library object */

    library = calloc(1, sizeof(*library));
    if (library == NULL)
        return errno;

    library->path = strdup(path);
    if (library->path == NULL) {
        library_close(&library);
        return ENOMEM;
    }

    *library_r = library;
    return 0;
}

void library_close(struct library **library_r) {
    struct library *library;

    assert(library_r != NULL);
    assert(*library_r != NULL);

    library = *library_r;
    *library_r = NULL;

    assert(library->ref == 0);

    if (library->path != NULL)
        free(library->path);

    if (library->plans != NULL) {
        unsigned i;

        for (i = 0; i < library->num_plans; ++i) {
            struct plan_entry *entry = &library->plans[i];
            if (entry->plan != NULL)
                free_plan(&entry->plan);
        }

        free(library->plans);
    }

    free(library);
}

static int find_plan_by_name(struct library *library, const char *name) {
    unsigned i;

    for (i = 0; i < library->num_plans; ++i)
        if (library->plans[i].plan != NULL &&
            strcmp(library->plans[i].plan->name, name) == 0)
            return (int)i;

    return -1;
}

static int find_plan_index(struct library *library,
                           const struct plan *plan) {
    unsigned i;

    for (i = 0; i < library->num_plans; ++i)
        if (library->plans[i].plan == plan)
            return (int)i;

    return -1;
}

static int is_valid_plan_name_char(char ch) {
    return (ch >= 'a' && ch <= 'z') ||
        (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') ||
        ch == '_' || ch == '-';
}

static int is_valid_plan_name(const char *name) {
    assert(name != NULL);

    do {
        if (!is_valid_plan_name_char(*name))
            return 0;
        ++name;
    } while (*name != 0);

    return 1;
}

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
            char **argv;

            if (plan->argv != NULL) {
                fprintf(stderr, "line %u: 'exec' already specified\n",
                        line_no);
                return -1;
            }

            assert(plan->argc == 0);

            plan->argv = calloc(256, sizeof(plan->argv[0]));
            if (plan->argv == NULL)
                return ENOMEM;

            while (value != NULL) {
                if (plan->argc >= 256) {
                    fprintf(stderr, "line %u: too many arguments\n",
                            line_no);
                    return -1;
                }

                plan->argv[plan->argc] = strdup(value);
                if (plan->argv[plan->argc] == NULL)
                    return ENOMEM;

                ++plan->argc;

                value = next_word(&p);
            }

            argv = realloc(plan->argv, plan->argc * sizeof(argv[0]));
            if (argv == NULL)
                return ENOMEM;

            plan->argv = argv;
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

    if (plan->argv == NULL) {
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

static int load_plan_config(const char *path, const char *name,
                            struct plan **plan_r) {
    struct plan *plan;
    FILE *file;
    int ret;

    assert(path != NULL);
    assert(is_valid_plan_name(name));

    plan = calloc(1, sizeof(*plan));
    if (plan == NULL)
        return ENOMEM;

    plan->uid = 65534;
    plan->gid = 65534;
    plan->priority = 10;

    plan->name = strdup(name);
    if (plan->name == NULL) {
        free_plan(&plan);
        return ENOMEM;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "failed to open file '%s': %s\n",
                path, strerror(errno));
        free_plan(&plan);
        return -1;
    }

    ret = parse_plan_config(plan, file);
    fclose(file);
    if (ret != 0) {
        fprintf(stderr, "parsing file '%s' failed\n", path);
        free_plan(&plan);
        return ret;
    }

    *plan_r = plan;
    return 0;
}

static int add_plan(struct library *library,
                    struct plan *plan,
                    time_t mtime) {
    struct plan_entry *entry;

    if (library->num_plans >= library->max_plans) {
        library->max_plans += 16;
        library->plans = realloc(library->plans,
                                 library->max_plans * sizeof(library->plans[0]));
        if (library->plans == NULL)
            abort();
    }

    entry = &library->plans[library->num_plans++];
    *entry = (struct plan_entry) {
        .plan = plan,
        .mtime = mtime,
    };

    return 0;
}

int library_get(struct library *library, const char *name,
                struct plan **plan_r) {
    int ret;
    char path[1024];
    struct stat st;
    struct plan_entry *entry;
    struct plan *plan;

    if (!is_valid_plan_name(name))
        return ENOENT;

    snprintf(path, sizeof(path), "%s/%s",
             library->path, name);
    ret = stat(path, &st);
    if (ret < 0) {
        if (ret != ENOENT)
            fprintf(stderr, "failed to stat '%s': %s\n",
                    path, strerror(errno));
        return errno;
    }

    ret = find_plan_by_name(library, name);
    if (ret >= 0) {
        /* plan is already in cache */

        entry = &library->plans[ret];
        plan = entry->plan;

        assert(plan != NULL);

        if (st.st_mtime != entry->mtime) {
            /* .. but it's not up to date */

            log(6, "reloading plan '%s'\n", name);

            ret = load_plan_config(path, name, &plan);
            if (ret != 0)
                return ret;

            plan->library = library;

            if (entry->plan->ref == 0)
                /* free memory of old plan only if there are no
                   references on it anymore */
                free_plan(&entry->plan);

            entry->mtime = st.st_mtime;
            entry->plan = plan;
        }
    } else {
        /* not in cache, load it from disk */

        log(6, "loading plan '%s'\n", name);
        
        ret = load_plan_config(path, name, &plan);
        if (ret != 0)
            return ret;

        plan->library = library;

        ret = add_plan(library, plan, st.st_mtime);
        if (ret != 0) {
            free_plan(&plan);
            return ret;
        }
    }

    if (plan->argc == 0 || plan->argv == NULL ||
        plan->argv[0] == NULL || plan->argv[0][0] == 0)
        return ENOENT;

    /* check if the executable exists; it would not if the Debian
       package has been deinstalled, but the plan's config file is
       still there */

    ret = stat(plan->argv[0], &st);
    if (ret < 0) {
        fprintf(stderr, "failed to stat '%s': %s\n",
                plan->argv[0], strerror(errno));
        return ENOENT;
    }

    *plan_r = plan;
    ++plan->ref;
    ++library->ref;
    return 0;
}

void plan_put(struct plan **plan_r) {
    struct plan *plan;
    struct library *library;

    assert(plan_r != NULL);
    assert(*plan_r != NULL);

    plan = *plan_r;
    *plan_r = NULL;

    library = plan->library;

    assert(plan->ref > 0);
    assert(library != NULL);
    assert(library->ref > 0);

    --plan->ref;
    --library->ref;

    if (plan->ref == 0) {
        /* free "old" plans which have refcount 0 */
        int i = find_plan_index(library, plan);
        if (i < 0)
            free_plan(&plan);
    }
}
