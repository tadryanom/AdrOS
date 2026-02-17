/* AdrOS head utility */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

static void head_fd(int fd, int nlines) {
    char buf[4096];
    int lines = 0;
    int r;
    while (lines < nlines && (r = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < r && lines < nlines; i++) {
            write(STDOUT_FILENO, &buf[i], 1);
            if (buf[i] == '\n') lines++;
        }
    }
}

int main(int argc, char** argv) {
    int nlines = 10;
    int start = 1;

    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'n' && argc > 2) {
        nlines = atoi(argv[2]);
        start = 3;
    } else if (argc > 1 && argv[1][0] == '-' && argv[1][1] >= '0' && argv[1][1] <= '9') {
        nlines = atoi(argv[1] + 1);
        start = 2;
    }

    if (start >= argc) {
        head_fd(STDIN_FILENO, nlines);
    } else {
        for (int i = start; i < argc; i++) {
            if (argc - start > 1) printf("==> %s <==\n", argv[i]);
            int fd = open(argv[i], O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "head: cannot open '%s'\n", argv[i]);
                continue;
            }
            head_fd(fd, nlines);
            close(fd);
        }
    }
    return 0;
}
