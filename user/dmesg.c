/* AdrOS dmesg utility — print kernel ring buffer from /proc/dmesg */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
    int fd = open("/proc/dmesg", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "dmesg: cannot open /proc/dmesg\n");
        return 1;
    }
    char buf[512];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(STDOUT_FILENO, buf, (size_t)n);
    close(fd);
    return 0;
}
