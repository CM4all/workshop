/*
 * $Id$
 *
 * Syslog network client.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

struct syslog_client;

int syslog_open(const char *me, const char *ident,
                int facility,
                const char *host_and_port,
                struct syslog_client **syslog_r);

void syslog_close(struct syslog_client **syslog_r);

int syslog_log(struct syslog_client *syslog, int priority, const char *msg);
