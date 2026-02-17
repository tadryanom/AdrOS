/* AdrOS free utility — display memory usage from /proc/meminfo */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
    int fd = open("/proc/meminfo", O_RDONLY);
    if (fd >= 0) {
        char buf[512];
        int n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) { buf[n] = '\0'; printf("%s", buf); }
        close(fd);
    } else {
        printf("free: /proc/meminfo not available\n");
    }
    return 0;
}
