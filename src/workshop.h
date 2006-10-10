/*
 * $Id$
 *
 * Internal declarations of cm4all-workshop.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "strarray.h"

#include <sys/types.h>

#define VERSION "0.1.0"


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

/** read configuration options from the command line */
void parse_cmdline(struct config *config, int argc, char **argv);

void config_dispose(struct config *config);


/* daemon.c */

int stdin_null(void);

void daemonize(struct config *config);

/* poll.c */

struct poll;
struct pollfd;

typedef void (*poll_callback_t)(struct pollfd *pollfd, void *ctx);

int poll_open(struct poll **poll_r);

void poll_close(struct poll **poll_r);

void poll_add(struct poll *poll, int fd, short events,
              poll_callback_t callback, void *ctx);

void poll_remove(struct poll *poll, int fd);

void poll_poll(struct poll *poll);

/* queue.c */

struct queue;

struct job {
    struct queue *queue;
    char *id, *plan_name, *syslog_server;
    struct strarray args;
};

int queue_open(const char *node_name,
               const char *conninfo, struct poll *poll,
               struct queue **queue_r);

void queue_close(struct queue **queue_r);

void queue_flush(struct queue *queue);

int queue_get(struct queue *queue, struct job **job_r);

int job_claim(struct job **job_r);

void job_skip(struct job **job_r);

int job_set_progress(struct job *job, unsigned progress);

int job_rollback(struct job **job_r);

int job_done(struct job **job_r, int status);

/* plan.c */

/** a library is a container for plan objects */
struct library;

/** a plan describes how to perform a specific job */
struct plan {
    struct library *library;
    char *name;
    char **argv;
    unsigned argc;
    char *chroot;
    uid_t uid;
    gid_t gid;
    int priority;
    unsigned ref;
};

int library_open(const char *path, struct library **library_r);

void library_close(struct library **library_r);

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
    char stdout_buffer[64];
    size_t stdout_length;

    int stderr_fd;
    char stderr_buffer[256];
    size_t stderr_length;
    struct syslog_client *syslog;
};

int workplace_open(const char *node_name, struct poll *p,
                   struct workplace **workplace_r);

void workplace_close(struct workplace **workplace_r);

int workplace_start(struct workplace *workplace,
                    struct job *job, struct plan *plan);

int workplace_is_empty(const struct workplace *workplace);

void workplace_waitpid(struct workplace *workplace);
