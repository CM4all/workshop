#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <grp.h>

#include "workshop.h"

int stdin_null(void) {
    int fd, ret;

    fd = open("/dev/null", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "failed to open /dev/null: %s", strerror(errno));
        return errno;
    }

    ret = dup2(fd, 0);
    if (ret < 0)
        abort();

    close(fd);

    return 0;
}

void daemonize(const struct config *config) {
    int ret, parentfd = -1, loggerfd = -1;
    pid_t logger_pid;

    /* daemonize */

    if (!config->no_daemon && getppid() != 1) {
        int fds[2];
        pid_t pid;

        ret = pipe(fds);
        if (ret < 0) {
            perror("pipe failed");
            exit(1);
        }

        pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(1);
        }

        if (pid > 0) {
            int status;
            fd_set rfds;
            char buffer[256];
            struct timeval tv;
            pid_t pid2;

            close(fds[1]);

            log(4, "waiting for daemon process %ld\n", (long)pid);

            do {
                FD_ZERO(&rfds);
                FD_SET(fds[0], &rfds);
                tv.tv_sec = 0;
                tv.tv_usec = 100000;
                ret = select(fds[0] + 1, &rfds, NULL, NULL, &tv);
                if (ret > 0 && read(fds[0], buffer, sizeof(buffer)) > 0) {
                    log(2, "detaching %ld\n", (long)getpid());
                    exit(0);
                }

                pid2 = waitpid(pid, &status, WNOHANG);
            } while (pid2 <= 0);

            log(3, "daemon process exited with %d\n",
                WEXITSTATUS(status));
            exit(WEXITSTATUS(status));
        }

        close(fds[0]);
        parentfd = fds[1];

        setsid();

        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);

        log(3, "daemonized as pid %ld\n", (long)getpid());
    }

    /* write PID file */

    if (config->pidfile != NULL) {
        FILE *file;

        file = fopen(config->pidfile, "w");
        if (file == NULL) {
            fprintf(stderr, "failed to create '%s': %s\n",
                    config->pidfile, strerror(errno));
            exit(1);
        }

        fprintf(file, "%ld\n", (long)getpid());
        fclose(file);
    }

    /* start logger process */

    if (config->logger != NULL) {
        int fds[2];

        log(3, "starting logger '%s'\n", config->logger);

        ret = pipe(fds);
        if (ret < 0) {
            perror("pipe failed");
            exit(1);
        }

        logger_pid = fork();
        if (logger_pid < 0) {
            perror("fork failed");
            exit(1);
        } else if (logger_pid == 0) {
            if (fds[0] != 0) {
                dup2(fds[0], 0);
                close(fds[0]);
            }

            close(fds[1]);
            close(1);
            close(2);

            execl("/bin/sh", "sh", "-c", config->logger, NULL);
            exit(1);
        }

        log(2, "logger started as pid %ld\n", (long)logger_pid);

        close(fds[0]);
        loggerfd = fds[1];

        log(3, "logger %ld connected\n", (long)logger_pid);
    }

    /* chdir */

    chdir("/");

    /* send parent process a signal */

    if (parentfd >= 0) {
        log(4, "closing parent pipe %d\n", parentfd);
        write(parentfd, &parentfd, sizeof(parentfd));
        close(parentfd);
    }

    /* now connect logger */

    if (loggerfd >= 0) {
        dup2(loggerfd, 1);
        dup2(loggerfd, 2);
        close(loggerfd);
    }
}
