/*
 * $Id$
 *
 * Internal declarations of cm4all-workshop.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "strarray.h"

#include <sys/types.h>
#include <event.h>


/* config.c */

struct config {
    const char *node_name;
    unsigned concurrency;
    const char *database;

    /* daemon config */
    int no_daemon;
    const char *pidfile, *logger;
};

extern int verbose;

#define log(level, ...) do { if (verbose >= (level)) { printf(__VA_ARGS__); fflush(stdout); } } while (0)

/** read configuration options from the command line */
void parse_cmdline(struct config *config, int argc, char **argv);

void config_dispose(struct config *config);


/* daemon.c */

int stdin_null(void);

void daemonize(struct config *config);

/* queue.c */

struct queue;

struct job {
    struct queue *queue;
    char *id, *plan_name, *syslog_server;
    struct strarray args;
};

typedef void (*queue_callback_t)(struct job *job, void *ctx);

int queue_open(const char *node_name, const char *conninfo,
               queue_callback_t callback, void *ctx,
               struct queue **queue_r);

void queue_close(struct queue **queue_r);

void queue_set_filter(struct queue *queue, const char *plans_include,
                      const char *plans_exclude);

int queue_run(struct queue *queue);

void queue_disable(struct queue *queue);

void queue_enable(struct queue *queue);

int job_set_progress(struct job *job, unsigned progress,
                     const char *timeout);

int job_rollback(struct job **job_r);

int job_done(struct job **job_r, int status);

/* plan.c */

/** a library is a container for plan objects */
struct library;

/** a plan describes how to perform a specific job */
struct plan {
    struct library *library;
    struct strarray argv;
    char *timeout, *chroot;
    uid_t uid;
    gid_t gid;
    int priority;
    unsigned ref;
};

int library_open(const char *path, struct library **library_r);

void library_close(struct library **library_r);

int library_update(struct library *library);

const char *library_plan_names(struct library *library);

int library_get(struct library *library, const char *name,
                struct plan **plan_r);

void plan_put(struct plan **plan_r);

/* operator.c */

struct workplace;

/** an operator is a job being executed */
struct operator {
    struct operator *next;
    struct workplace *workplace;
    struct job *job;
    struct plan *plan;
    pid_t pid;

    int stdout_fd;
    struct event stdout_event;
    char stdout_buffer[64];
    size_t stdout_length;
    unsigned progress;

    int stderr_fd;
    struct event stderr_event;
    char stderr_buffer[512];
    size_t stderr_length;
    struct syslog_client *syslog;
};

int workplace_open(const char *node_name, unsigned max_operators,
                   struct workplace **workplace_r);

void workplace_close(struct workplace **workplace_r);

const char *workplace_plan_names(struct workplace *workplace);

int workplace_start(struct workplace *workplace,
                    struct job *job, struct plan *plan);

int workplace_is_empty(const struct workplace *workplace);

int workplace_is_full(const struct workplace *workplace);

void workplace_waitpid(struct workplace *workplace);
