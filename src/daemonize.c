#include "daemonize.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <syslog.h>
#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

int daemon_init(void) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(0);

    if (setsid() < 0) return -1;

    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(0);

    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_RDWR);
    return 0;
}

void daemon_notify_ready(void) {
#ifdef HAVE_SYSTEMD
    sd_notify(0, "READY=1");
#endif
}

void daemon_notify_status(const char *status) {
#ifdef HAVE_SYSTEMD
    if (status)
        sd_notifyf(0, "STATUS=%s", status);
#endif
}

void daemon_notify_watchdog(void) {
#ifdef HAVE_SYSTEMD
    sd_notify(0, "WATCHDOG=1");
#endif
}
