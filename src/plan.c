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
    int deinstalled;
    time_t mtime, disabled_until;
    unsigned generation;
};

struct library {
    char *path;

    struct plan_entry *plans;
    unsigned num_plans, max_plans;
    time_t next_plans_check;
    unsigned generation;

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

static int plan_is_disabled(const struct plan_entry *entry, time_t now) {
    return entry->disabled_until > 0 && now < entry->disabled_until;
}

static int update_plan_names(struct library *library) {
    const time_t now = time(NULL);
    struct strarray plan_names;
    unsigned i;
    const struct plan_entry *entry;

    if (library->plan_names != NULL && now < library->next_update)
        return 0;

    library->next_update = now + 60;

    /* collect new list */

    strarray_init(&plan_names);

    for (i = 0; i < library->num_plans; ++i) {
        entry = &library->plans[i];

        if (!plan_is_disabled(entry, now))
            strarray_append(&plan_names, entry->name);
    }

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
        if (strcmp(library->plans[i].name, name) == 0)
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

static void disable_plan(struct library *library,
                         struct plan_entry *entry,
                         time_t duration) {
    entry->disabled_until = time(NULL) + duration;
    library->next_update = 0;
}

static struct plan_entry *make_plan_entry(struct library *library, const char *name) {
    int ret;

    ret = find_plan_by_name(library, name);
    if (ret >= 0)
        return &library->plans[ret];

    return add_plan_entry(library, name);
}

static int check_plan_mtime(struct library *library, struct plan_entry *entry) {
    int ret;
    char path[1024];
    struct stat st;

    snprintf(path, sizeof(path), "%s/%s",
             library->path, entry->name);
    ret = stat(path, &st);
    if (ret < 0) {
        if (ret != ENOENT)
            fprintf(stderr, "failed to stat '%s': %s\n",
                    path, strerror(errno));

        entry->mtime = 0;

        return errno;
    }

    if (!S_ISREG(st.st_mode)) {
        if (entry->plan != NULL) {
            /* free memory of old plan only if there are no
               references on it anymore */
            if (entry->plan->ref == 0)
                free_plan(&entry->plan);
            else
                entry->plan = NULL;
        }

        entry->mtime = 0;

        disable_plan(library, entry, 60);
        return ENOENT;
    }

    if (st.st_mtime != entry->mtime) {
        entry->disabled_until = 0;

        if (entry->plan != NULL) {
            /* free memory of old plan only if there are no
               references on it anymore */
            if (entry->plan->ref == 0)
                free_plan(&entry->plan);
            else
                entry->plan = NULL;
        }

        entry->mtime = st.st_mtime;
    }

    if (entry->disabled_until > 0) {
        if (time(NULL) < entry->disabled_until)
            /* this plan is temporarily disabled due to previous errors */
            return ENOENT;

        entry->disabled_until = 0;
    }

    return 0;
}

static int validate_plan(struct plan_entry *entry) {
    const struct plan *plan = entry->plan;
    int ret;
    struct stat st;

    assert(plan != NULL);
    assert(plan->argv.num > 0);
    assert(plan->argv.values[0] != NULL && plan->argv.values[0][0] != 0);
    assert(plan->library != NULL);

    /* check if the executable exists; it would not if the Debian
       package has been deinstalled, but the plan's config file is
       still there */

    ret = stat(plan->argv.values[0], &st);
    if (ret < 0) {
        if (errno != ENOENT || !entry->deinstalled)
            fprintf(stderr, "failed to stat '%s': %s\n",
                    plan->argv.values[0], strerror(errno));
        if (errno == ENOENT)
            entry->deinstalled = 1;
        else
            disable_plan(plan->library, entry, 60);
        return ENOENT;
    }

    entry->deinstalled = 0;

    return 0;
}

static int load_plan_entry(struct library *library,
                           struct plan_entry *entry) {
    int ret;
    char path[1024];

    assert(entry->name != NULL);
    assert(entry->plan == NULL);
    assert(entry->mtime != 0);

    log(6, "loading plan '%s'\n", entry->name);

    snprintf(path, sizeof(path), "%s/%s",
             library->path, entry->name);

    ret = load_plan_config(path, entry->name, &entry->plan);
    if (ret != 0) {
        disable_plan(library, entry, 600);
        return ret;
    }

    entry->plan->library = library;

    library->next_update = 0;

    return 0;
}

static int library_update_plan(struct library *library,
                               struct plan_entry *entry) {
    int ret;

    ret = check_plan_mtime(library, entry);
    if (ret != 0)
        return ret;

    if (entry->plan == NULL) {
        ret = load_plan_entry(library, entry);
        if (ret != 0)
            return ret;
    }

    ret = validate_plan(entry);
    if (ret != 0)
        return ret;

    return 0;
}

static void library_remove_plan(struct library *library, unsigned i) {
    struct plan_entry *entry;

    assert(i < library->num_plans);

    entry = &library->plans[i];

    if (entry->name != NULL)
        free(entry->name);

    if (entry->plan != NULL && entry->plan->ref == 0)
        free_plan(&entry->plan);

    --library->num_plans;
    memmove(entry, entry + 1,
            sizeof(*entry) * (library->num_plans - i));
}

static int library_update_plans(struct library *library) {
    DIR *dir;
    struct dirent *ent;
    unsigned i;

    /* read list of plans from file system, update our list */

    dir = opendir(library->path);
    if (dir == NULL) {
        fprintf(stderr, "failed to opendir '%s': %s\n",
                library->path, strerror(errno));
        return -1;
    }

    ++library->generation;

    while ((ent = readdir(dir)) != NULL) {
        struct plan_entry *entry;

        if (!is_valid_plan_name(ent->d_name))
            continue;

        entry = make_plan_entry(library, ent->d_name);
        assert(entry != NULL);

        library_update_plan(library, entry);
        entry->generation = library->generation;
    }

    closedir(dir);

    /* remove all plans */

    for (i = library->num_plans; i > 0;) {
        struct plan_entry *entry = &library->plans[--i];

        if (entry->generation != library->generation) {
            library_remove_plan(library, i);
            library->next_update = 0;
        }
    }

    return 0;
}

int library_update(struct library *library) {
    const time_t now = time(NULL);
    int ret;
    struct stat st;

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

    if (st.st_mtime == library->mtime && now < library->next_plans_check)
        return 0;

    /* do it */

    ret = library_update_plans(library);
    if (ret != 0)
        return ret;
        
    /* update mtime */

    library->mtime = st.st_mtime;
    library->next_plans_check = now + 60;

    return 0;
}

int library_get(struct library *library, const char *name,
                struct plan **plan_r) {
    int ret;
    struct plan_entry *entry;

    if (!is_valid_plan_name(name))
        return ENOENT;

    ret = find_plan_by_name(library, name);
    if (ret < 0)
        return ENOENT;

    entry = &library->plans[ret];

    ret = library_update_plan(library, entry);
    if (ret != 0)
        return ret;

    *plan_r = entry->plan;
    ++entry->plan->ref;
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
