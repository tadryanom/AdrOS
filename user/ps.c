/* AdrOS ps utility — list processes from /proc */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

static int is_digit(char c) { return c >= '0' && c <= '9'; }

int main(void) {
    printf("  PID CMD\n");
    int fd = open("/proc", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ps: cannot open /proc\n");
        return 1;
    }
    char buf[512];
    int rc;
    while ((rc = getdents(fd, buf, sizeof(buf))) > 0) {
        int off = 0;
        while (off < rc) {
            struct dirent* d = (struct dirent*)(buf + off);
            if (d->d_reclen == 0) break;
            if (is_digit(d->d_name[0])) {
                char path[64];
                snprintf(path, sizeof(path), "/proc/%s/cmdline", d->d_name);
                int cfd = open(path, O_RDONLY);
                char cmd[64] = "?";
                if (cfd >= 0) {
                    int n = read(cfd, cmd, sizeof(cmd) - 1);
                    if (n > 0) {
                        cmd[n] = '\0';
                        while (n > 0 && (cmd[n-1] == '\n' || cmd[n-1] == '\0')) {
                            cmd[--n] = '\0';
                        }
                    }
                    if (n <= 0) strcpy(cmd, "[kernel]");
                    close(cfd);
                }
                printf("%5s %s\n", d->d_name, cmd);
            }
            off += d->d_reclen;
        }
    }
    close(fd);
    return 0;
}
