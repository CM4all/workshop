/*
 * Manage operator processes.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "workplace.hxx"
#include "operator.hxx"
#include "debug.h"
#include "plan.hxx"
#include "queue.hxx"

extern "C" {
#include "syslog.h"
#include "pg-util.h"
}

#include <daemon/log.h>

#include <map>
#include <string>

#include <glib.h>

#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <poll.h>
#include <grp.h>
#include <sys/time.h>
#include <sys/resource.h>

int workplace_open(const char *node_name, unsigned max_operators,
                   struct workplace **workplace_r) {
    struct workplace *workplace;

    assert(max_operators > 0);

    workplace = (struct workplace*)calloc(1, sizeof(*workplace));
    if (workplace == NULL)
        return errno;

    workplace->node_name = node_name;
    if (workplace->node_name == NULL) {
        workplace_close(&workplace);
        return ENOMEM;
    }

    workplace->max_operators = max_operators;

    *workplace_r = workplace;
    return 0;
}

void workplace_close(struct workplace **workplace_r) {
    struct workplace *workplace;

    assert(workplace_r != NULL);
    assert(*workplace_r != NULL);

    workplace = *workplace_r;
    *workplace_r = NULL;

    assert(workplace->head == NULL);
    assert(workplace->num_operators == 0);

    if (workplace->plan_names != NULL)
        free(workplace->plan_names);

    if (workplace->full_plan_names != NULL)
        free(workplace->full_plan_names);

    free(workplace);
}

int workplace_plan_is_running(const struct workplace *workplace,
                              const struct plan *plan) {
    for (struct Operator *o = workplace->head; o != NULL; o = o->next)
        if (o->plan == plan)
            return 1;

    return 0;
}

const char *workplace_plan_names(struct workplace *workplace) {
    struct strarray plan_names;

    strarray_init(&plan_names);

    for (struct Operator *o = workplace->head; o != NULL; o = o->next)
        if (!strarray_contains(&plan_names, o->job->plan_name))
            strarray_append(&plan_names, o->job->plan_name);

    if (workplace->plan_names != NULL)
        free(workplace->plan_names);

    workplace->plan_names = pg_encode_array(&plan_names);

    strarray_free(&plan_names);

    return workplace->plan_names == NULL
        ? "{}"
        : workplace->plan_names;
}

struct plan_counter {
    const struct plan *plan;
    const char *plan_name;
    unsigned num;
};

const char *workplace_full_plan_names(struct workplace *workplace) {
    struct strarray plan_names;
    struct plan_counter *counters;
    unsigned num_counters = 0, i;

    if (workplace->num_operators == 0)
        return "{}";

    strarray_init(&plan_names);

    counters = (struct plan_counter *)
        calloc(workplace->num_operators, sizeof(counters[0]));
    if (counters == NULL)
        abort();

    for (struct Operator *o = workplace->head; o != NULL; o = o->next) {
        if (o->plan->concurrency == 0)
            continue;

        for (i = 0; i < num_counters; ++i)
            if (counters[i].plan == o->plan)
                break;

        if (i == num_counters) {
            ++num_counters;
            counters[i].plan = o->plan;
            counters[i].plan_name = o->job->plan_name;
        }

        ++counters[i].num;

        assert(counters[i].plan->concurrency == 0 ||
               counters[i].num <= counters[i].plan->concurrency ||
               strarray_contains(&plan_names, counters[i].plan_name));

        if (counters[i].num == counters[i].plan->concurrency)
            strarray_append(&plan_names, counters[i].plan_name);
    }

    free(counters);

    if (workplace->full_plan_names != NULL)
        free(workplace->full_plan_names);

    workplace->full_plan_names = pg_encode_array(&plan_names);

    strarray_free(&plan_names);

    return workplace->full_plan_names == NULL
        ? "{}"
        : workplace->full_plan_names;
}

static void free_operator(struct Operator **operator_r) {
    assert(operator_r != NULL);
    assert(*operator_r != NULL);

    struct Operator *o = *operator_r;
    *operator_r = NULL;

    if (o->stdout_fd >= 0) {
        event_del(&o->stdout_event);
        close(o->stdout_fd);
    }

    if (o->stderr_fd >= 0) {
        event_del(&o->stderr_event);
        close(o->stderr_fd);
    }

    if (o->syslog != NULL)
        syslog_close(&o->syslog);

    free(o);
}

static void
stdout_callback(G_GNUC_UNUSED int fd, G_GNUC_UNUSED short event, void *ctx)
{
    struct Operator *o = (struct Operator*)ctx;
    char buffer[512];
    ssize_t nbytes, i;
    unsigned progress = 0, p;

    nbytes = read(o->stdout_fd, buffer, sizeof(buffer));
    if (nbytes <= 0) {
        event_del(&o->stdout_event);
        close(o->stdout_fd);
        o->stdout_fd = -1;
        return;
    }

    for (i = 0; i < nbytes; ++i) {
        char ch = buffer[i];

        if (ch >= '0' && ch <= '9' &&
            o->stdout_length < sizeof(o->stdout_buffer) - 1) {
            o->stdout_buffer[o->stdout_length++] = ch;
        } else {
            if (o->stdout_length > 0) {
                o->stdout_buffer[o->stdout_length] = 0;
                p = (unsigned)strtoul(o->stdout_buffer, NULL, 10);
                if (p <= 100)
                    progress = p;
            }

            o->stdout_length = 0;
        }
    }

    if (progress > 0 && progress != o->progress) {
        job_set_progress(o->job, progress, o->plan->timeout);
        o->progress = progress;
    }
}

static void
stderr_callback(G_GNUC_UNUSED int fd, G_GNUC_UNUSED short event, void *ctx)
{
    struct Operator *o = (struct Operator*)ctx;
    char buffer[512];
    ssize_t nbytes, i;

    assert(o->syslog != NULL);

    nbytes = read(o->stderr_fd, buffer, sizeof(buffer));
    if (nbytes <= 0) {
        event_del(&o->stderr_event);
        close(o->stderr_fd);
        o->stderr_fd = -1;
        return;
    }

    for (i = 0; i < nbytes; ++i) {
        char ch = buffer[i];

        if (ch == '\r' || ch == '\n') {
            if (o->stderr_length > 0) {
                o->stderr_buffer[o->stderr_length] = 0;
                syslog_log(o->syslog, 6, o->stderr_buffer);
            }

            o->stderr_length = 0;
        } else if (ch > 0 && (ch & ~0x7f) == 0 &&
                   o->stderr_length < sizeof(o->stderr_buffer) - 1) {
            o->stderr_buffer[o->stderr_length++] = ch;
        }
    }
}

static int splice_string(char **pp, size_t start, size_t end,
                         const char *replacement) {
    char *p = *pp, *n;
    size_t end_length = strlen(p + end);
    size_t replacement_length = strlen(replacement);

    assert(end >= start);
    assert(replacement != NULL);

    n = (char*)malloc(start + replacement_length + end_length + 1);
    if (n == NULL)
        return errno;

    memcpy(n, p, start);
    memcpy(n + start, replacement, replacement_length);
    memcpy(n + start + replacement_length, p + end, end_length);
    n[start + replacement_length + end_length] = 0;

    free(p);
    *pp = n;
    return 0;
}

typedef std::map<std::string, std::string> StringMap;

static int
expand_vars(char **pp, const StringMap &vars)
{
    int ret;
    char *v = *pp, *p = v, *dollar, *end;

    while (1) {
        dollar = strchr(p, '$');
        if (dollar == NULL)
            break;

        if (dollar[1] == '{') {
            end = strchr(dollar + 2, '}');
            if (end == NULL)
                break;

            const std::string key(dollar + 2, end);
            auto i = vars.find(key);
            const char *expanded = i != vars.end()
                ? i->second.c_str()
                : "";

            ret = splice_string(pp, dollar - v, end + 1 - v, expanded);
            if (ret != 0)
                return ret;

            p = *pp + (p - v) + strlen(expanded);
            v = *pp;
        } else {
            ++p;
        }
    }

    return 0;
}

static int
expand_operator_vars(const struct Operator *o,
                     struct strarray *argv)
{
    StringMap vars;
    vars.insert(std::make_pair("0", argv->values[0]));
    vars.insert(std::make_pair("NODE", o->workplace->node_name));
    vars.insert(std::make_pair("JOB", o->job->id));
    vars.insert(std::make_pair("PLAN", o->job->plan_name));

    for (unsigned i = 1; i < argv->num; ++i) {
        assert(argv->values[i] != NULL);
        int ret = expand_vars(&argv->values[i], vars);
        if (ret != 0)
            return ret;
    }

    return 0;
}

int workplace_start(struct workplace *workplace,
                    struct job *job, struct plan *plan) {
    int ret, stdout_fds[2], stderr_fds[2];
    struct strarray argv;
    unsigned i;

    assert(plan != NULL);
    assert(plan->argv.num > 0);

    /* create operator object */

    struct Operator *o = (struct Operator *)calloc(1, sizeof(*o));
    if (o == NULL)
        return errno;

    o->workplace = workplace;
    o->stdout_fd = -1;
    o->stderr_fd = -1;
    o->job = job;
    o->plan = plan;

    ret = pipe(stdout_fds);
    if (ret < 0) {
        fprintf(stderr, "pipe() failed: %s\n", strerror(errno));
        free_operator(&o);
        return -1;
    }

    /* create stdout/stderr pipes */

    o->stdout_fd = stdout_fds[0];
    event_set(&o->stdout_event, o->stdout_fd,
              EV_READ|EV_PERSIST, stdout_callback, o);
    event_add(&o->stdout_event, NULL);

    if (job->syslog_server != NULL) {
        char ident[256];

        snprintf(ident, sizeof(ident), "%s[%s]",
                 job->plan_name, job->id);

        ret = syslog_open(workplace->node_name, ident, 1,
                          job->syslog_server,
                          &o->syslog);
        if (ret != 0) {
            if (ret > 0)
                fprintf(stderr, "syslog_open(%s) failed: %s\n",
                        job->syslog_server, strerror(ret));
            free_operator(&o);
            close(stdout_fds[1]);
            return -1;
        }

        ret = pipe(stderr_fds);
        if (ret < 0) {
            fprintf(stderr, "pipe() failed: %s\n", strerror(errno));
            free_operator(&o);
            close(stdout_fds[1]);
            return -1;
        }

        o->stderr_fd = stderr_fds[0];
        event_set(&o->stderr_event, o->stderr_fd,
                  EV_READ|EV_PERSIST, stderr_callback, o);
        event_add(&o->stderr_event, NULL);
    }

    /* build command line */

    strarray_init(&argv);
    for (i = 0; i < plan->argv.num; ++i)
        strarray_append(&argv, plan->argv.values[i]);

    for (i = 0; i < job->args.num; ++i)
        strarray_append(&argv, job->args.values[i]);

    ret = expand_operator_vars(o, &argv);
    if (ret != 0) {
        strarray_free(&argv);
        free_operator(&o);
        close(stdout_fds[1]);
        if (job->syslog_server != NULL)
            close(stderr_fds[1]);
        return -1;
    }

    strarray_append(&argv, NULL);

    /* fork */

    o->pid = fork();
    if (o->pid < 0) {
        fprintf(stderr, "fork() failed: %s\n", strerror(errno));
        free_operator(&o);
        close(stdout_fds[1]);
        if (job->syslog_server != NULL)
            close(stderr_fds[1]);
        return -1;
    }

    if (o->pid == 0) {
        /* in the operator process */

        clearenv();

        /* swap effective uid back to root */

        if (!debug_mode) {
            ret = setreuid(0, 0);
            if (ret < 0) {
                perror("setreuid() to root failed");
                exit(1);
            }
        }

        /* chroot */

        if (plan->chroot != NULL) {
            ret = chroot(plan->chroot);
            fprintf(stderr, "chroot('%s') failed: %s\n",
                    plan->chroot, strerror(errno));
            exit(1);
        }

        /* priority */

        ret = setpriority(PRIO_PROCESS, getpid(), plan->priority);
        if (ret < 0) {
            fprintf(stderr, "setpriority() failed: %s\n", strerror(errno));
            exit(1);
        }

        /* UID / GID */

        if (!debug_mode) {
            ret = setgroups(plan->num_groups, plan->groups);
            if (ret < 0) {
                fprintf(stderr, "setgroups() failed: %s\n", strerror(errno));
                exit(1);
            }

            ret = setregid(plan->gid, plan->gid);
            if (ret < 0) {
                fprintf(stderr, "setregid() failed: %s\n", strerror(errno));
                exit(1);
            }

            ret = setreuid(plan->uid, plan->uid);
            if (ret < 0) {
                fprintf(stderr, "setreuid() failed: %s\n", strerror(errno));
                exit(1);
            }
        }

        /* connect pipes */

        dup2(stdout_fds[1], 1);
        if (job->syslog_server != NULL)
            dup2(stderr_fds[1], 2);

        close(stdout_fds[0]);
        close(stdout_fds[1]);
        if (job->syslog_server != NULL) {
            close(stderr_fds[0]);
            close(stderr_fds[1]);
        }

        /* session */

        setsid();

        /* execute plan */

        execv(argv.values[0], argv.values);
        fprintf(stderr, "execv() failed: %s\n", strerror(errno));
        exit(1);
    }

    strarray_free(&argv);

    close(stdout_fds[1]);
    if (job->syslog_server != NULL)
        close(stderr_fds[1]);

    o->next = workplace->head;
    workplace->head = o;
    ++workplace->num_operators;

    daemon_log(2, "job %s (plan '%s') running as pid %d\n",
               job->id, job->plan_name, o->pid);

    return 0;
}

int workplace_is_empty(const struct workplace *workplace) {
    return workplace->head == NULL;
}

int workplace_is_full(const struct workplace *workplace) {
    return workplace->num_operators >= workplace->max_operators;
}

static struct Operator **find_operator_by_pid(struct workplace *workplace,
                                              pid_t pid) {
    struct Operator **operator_p = &workplace->head;

    while (1) {
        struct Operator *o = *operator_p;
        if (o == NULL)
            return NULL;

        if (o->pid == pid)
            return operator_p;

        operator_p = &o->next;
    }
}

void workplace_waitpid(struct workplace *workplace) {
    pid_t pid;
    int status, exit_status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        struct Operator **operator_p = find_operator_by_pid(workplace, pid);
        if (operator_p == NULL)
            continue;

        struct Operator *o = *operator_p;
        assert(o != NULL);

        exit_status = WEXITSTATUS(status);

        if (WIFSIGNALED(status)) {
            daemon_log(1, "job %s (pid %d) died from signal %d%s\n",
                       o->job->id, pid,
                       WTERMSIG(status),
                       WCOREDUMP(status) ? " (core dumped)" : "");
            exit_status = -1;
        } else if (exit_status == 0)
            daemon_log(3, "job %s (pid %d) exited with success\n",
                       o->job->id, pid);
        else
            daemon_log(2, "job %s (pid %d) exited with status %d\n",
                       o->job->id, pid,
                       exit_status);

        plan_put(&o->plan);

        job_done(&o->job, exit_status);

        --workplace->num_operators;
        *operator_p = o->next;
        free_operator(&o);
    }
}
