/* AdrOS cat utility */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

static void cat_fd(int fd) {
    char buf[4096];
    int r;
    while ((r = read(fd, buf, sizeof(buf))) > 0) {
        write(STDOUT_FILENO, buf, (size_t)r);
    }
}

int main(int argc, char** argv) {
    if (argc <= 1) {
        cat_fd(STDIN_FILENO);
        return 0;
    }

    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-") == 0) {
            cat_fd(STDIN_FILENO);
            continue;
        }
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "cat: %s: No such file or directory\n", argv[i]);
            rc = 1;
            continue;
        }
        cat_fd(fd);
        close(fd);
    }
    return rc;
}
