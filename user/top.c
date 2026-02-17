/* AdrOS top utility — one-shot process listing with basic info */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

static int is_digit(char c) { return c >= '0' && c <= '9'; }

int main(void) {
    printf("  PID  STATE CMD\n");
    int fd = open("/proc", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "top: cannot open /proc\n");
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

                /* Read cmdline */
                snprintf(path, sizeof(path), "/proc/%s/cmdline", d->d_name);
                int cfd = open(path, O_RDONLY);
                char cmd[64] = "[kernel]";
                if (cfd >= 0) {
                    int n = read(cfd, cmd, sizeof(cmd) - 1);
                    if (n > 0) {
                        cmd[n] = '\0';
                        while (n > 0 && (cmd[n-1] == '\n' || cmd[n-1] == '\0')) cmd[--n] = '\0';
                    }
                    if (n <= 0) strcpy(cmd, "[kernel]");
                    close(cfd);
                }

                /* Read status for state */
                snprintf(path, sizeof(path), "/proc/%s/status", d->d_name);
                int sfd = open(path, O_RDONLY);
                char state[16] = "?";
                if (sfd >= 0) {
                    char sbuf[256];
                    int sn = read(sfd, sbuf, sizeof(sbuf) - 1);
                    if (sn > 0) {
                        sbuf[sn] = '\0';
                        char* st = strstr(sbuf, "State:");
                        if (st) {
                            st += 6;
                            while (*st == ' ' || *st == '\t') st++;
                            int si = 0;
                            while (*st && *st != '\n' && si < 15) state[si++] = *st++;
                            state[si] = '\0';
                        }
                    }
                    close(sfd);
                }

                printf("%5s %6s %s\n", d->d_name, state, cmd);
            }
            off += d->d_reclen;
        }
    }
    close(fd);
    return 0;
}
