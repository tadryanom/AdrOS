/* AdrOS uptime utility */
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    /* Try /proc/uptime first */
    int fd = open("/proc/uptime", O_RDONLY);
    if (fd >= 0) {
        char buf[64];
        int r = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (r > 0) {
            buf[r] = '\0';
            printf("up %s", buf);
            return 0;
        }
    }

    /* Fallback: use CLOCK_MONOTONIC */
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        fprintf(stderr, "uptime: cannot get time\n");
        return 1;
    }

    unsigned long sec = ts.tv_sec;
    unsigned long days = sec / 86400;
    unsigned long hours = (sec % 86400) / 3600;
    unsigned long mins = (sec % 3600) / 60;
    unsigned long secs = sec % 60;

    printf("up");
    if (days > 0) printf(" %lu day%s,", days, days > 1 ? "s" : "");
    printf(" %02lu:%02lu:%02lu\n", hours, mins, secs);
    return 0;
}
