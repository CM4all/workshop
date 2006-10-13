/*
 * $Id$
 *
 * Manage the list of plans (= library) and load their configuration
 * files.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "workshop.h"
#include "pg-util.h"

#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <time.h>
#include <dirent.h>

struct plan_entry {
    char *name;
    struct plan *plan;
    time_t mtime;
};

struct library {
    char *path;
    struct plan_entry *plans;
    unsigned num_plans, max_plans;
    unsigned ref;
    char *plan_names;
    time_t mtime, next_update;
};

static void free_plan(struct plan **plan_r) {
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
            if (entry->name != NULL)
                free(entry->name);
            if (entry->plan != NULL)
                free_plan(&entry->plan);
        }

        free(library->plans);
    }

    if (library->plan_names)
        free(library->plan_names);

    free(library);
}

static int is_valid_plan_name(const char *name);

static int update_plan_names(struct library *library) {
    const time_t now = time(NULL);
    int ret;
    struct stat st;
    DIR *dir;
    struct dirent *ent;
    char path[1024];
    struct strarray plan_names;

    /* check directory time stamp */

    ret = stat(library->path, &st);
    if (ret < 0) {
        fprintf(stderr, "failed to stat '%s': %s\n",
                library->path, strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "not a directory: %s\n", library->path);
        return -1;
    }

    if (st.st_mtime == library->mtime &&
        now < library->next_update) {
        assert(library->plan_names != NULL);
        return 0;
    }

    log(6, "updating plan list\n");

    library->mtime = st.st_mtime;
    library->next_update = now + 60;

    /* read directory */

    strarray_init(&plan_names);

    dir = opendir(library->path);
    if (dir == NULL) {
        fprintf(stderr, "failed to opendir '%s': %s\n",
                library->path, strerror(errno));
        return -1;
    }

    while ((ent = readdir(dir)) != NULL) {
        if (!is_valid_plan_name(ent->d_name))
            continue;

        snprintf(path, sizeof(path), "%s/%s", library->path, ent->d_name);
        ret = stat(path, &st);
        if (ret < 0) {
            fprintf(stderr, "failed to stat '%s': %s\n",
                    path, strerror(errno));
            continue;
        }

        if (!S_ISREG(st.st_mode))
            continue;

        strarray_append(&plan_names, ent->d_name);
    }

    closedir(dir);

    if (library->plan_names != NULL)
        free(library->plan_names);

    library->plan_names = pg_encode_array(&plan_names);

    strarray_free(&plan_names);

    return 0;
}

const char *library_plan_names(struct library *library) {
    update_plan_names(library);

    return library->plan_names == NULL
        ? "{}"
        : library->plan_names;
}

static int find_plan_by_name(struct library *library, const char *name) {
    unsigned i;

    for (i = 0; i < library->num_plans; ++i)
        if (library->plans[i].plan != NULL &&
            strcmp(library->plans[i].name, name) == 0)
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
            if (plan->argv.num > 0) {
                fprintf(stderr, "line %u: 'exec' already specified\n",
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

static struct plan_entry *add_plan_entry(struct library *library,
                                         const char *name) {
    struct plan_entry *entry;

    assert(is_valid_plan_name(name));

    if (library->num_plans >= library->max_plans) {
        library->max_plans += 16;
        library->plans = realloc(library->plans,
                                 library->max_plans * sizeof(library->plans[0]));
        if (library->plans == NULL)
            abort();
    }

    entry = &library->plans[library->num_plans++];
    memset(entry, 0, sizeof(*entry));
    entry->name = strdup(name);
    if (entry->name == NULL)
        abort();

    return entry;
}

static int add_plan(struct library *library,
                    const char *name,
                    struct plan *plan,
                    time_t mtime) {
    struct plan_entry *entry;

    entry = add_plan_entry(library, name);
    entry->plan = plan;
    entry->mtime = mtime;

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

        ret = add_plan(library, name, plan, st.st_mtime);
        if (ret != 0) {
            free_plan(&plan);
            return ret;
        }
    }

    if (plan->argv.num == 0 ||
        plan->argv.values[0] == NULL || plan->argv.values[0][0] == 0)
        return ENOENT;

    /* check if the executable exists; it would not if the Debian
       package has been deinstalled, but the plan's config file is
       still there */

    ret = stat(plan->argv.values[0], &st);
    if (ret < 0) {
        fprintf(stderr, "failed to stat '%s': %s\n",
                plan->argv.values[0], strerror(errno));
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
