/*
 * $Id$
 *
 * Internal declarations of cm4all-workshop.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

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

struct job {
    char *id, *plan_name, *syslog_server;
    char **args;
    unsigned num_args;
};

struct queue;

int queue_open(const char *node_name,
               const char *conninfo, struct poll *poll,
               struct queue **queue_r);

void queue_close(struct queue **queue_r);

void queue_flush(struct queue *queue);

int queue_get(struct queue *queue, struct job **job_r);

int queue_claim(struct queue *queue, struct job **job_r);

void queue_skip(struct queue *queue, struct job **job_r);

int queue_rollback(struct queue *queue, struct job **job_r);

int queue_done(struct queue *queue, struct job **job_r, int status);

/* plan.c */

/** a library is a container for plan objects */
struct library;

/** a plan describes how to perform a specific job */
struct plan {
    char *name;
    char **argv;
    unsigned argc;
    uid_t uid;
    gid_t gid;
    int priority;
    unsigned ref;
};

int library_open(const char *path, struct library **library_r);

void library_close(struct library **library_r);

int library_get(struct library *library, const char *name,
                struct plan **plan_r);

void library_put(struct library *library,
                 struct plan **plan_r);

/* operator.c */

/** an operator is a job being executed */
struct operator {
    struct operator *next;
    struct queue *queue;
    struct job *job;
    struct library *library;
    struct plan *plan;
    pid_t pid;
    int stderr_fd, stdout_fd;
};

struct workplace;

int workplace_open(struct poll *p, struct workplace **workplace_r);

void workplace_close(struct workplace **workplace_r);

int workplace_start(struct workplace *workplace,
                    struct queue *queue, struct job *job,
                    struct library *library,
                    struct plan *plan);

int workplace_is_empty(const struct workplace *workplace);

void workplace_waitpid(struct workplace *workplace);
