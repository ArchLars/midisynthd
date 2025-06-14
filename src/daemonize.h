#ifndef MIDISYNTHD_DAEMONIZE_H
#define MIDISYNTHD_DAEMONIZE_H

int daemon_init(void);
void daemon_notify_ready(void);
void daemon_notify_status(const char *status);
void daemon_notify_watchdog(void);

#endif /* MIDISYNTHD_DAEMONIZE_H */
