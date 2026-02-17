/* AdrOS hostname utility */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    /* Try /proc/hostname first, then /etc/hostname, then fallback */
    static const char* paths[] = { "/proc/hostname", "/etc/hostname", NULL };
    for (int i = 0; paths[i]; i++) {
        int fd = open(paths[i], O_RDONLY);
        if (fd >= 0) {
            char buf[256];
            int r = read(fd, buf, sizeof(buf) - 1);
            close(fd);
            if (r > 0) {
                buf[r] = '\0';
                /* Strip trailing newline */
                if (r > 0 && buf[r - 1] == '\n') buf[r - 1] = '\0';
                printf("%s\n", buf);
                return 0;
            }
        }
    }

    printf("adros\n");
    return 0;
}
