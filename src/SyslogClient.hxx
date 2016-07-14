/*
 * Syslog network client.
 *
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SYSLOG_CLIENT_HXX
#define SYSLOG_CLIENT_HXX

class SyslogClient;

int syslog_open(const char *me, const char *ident,
                int facility,
                const char *host_and_port,
                SyslogClient **syslog_r);

void syslog_close(SyslogClient **syslog_r);

int syslog_log(SyslogClient *syslog, int priority, const char *msg);

#endif
