/* AdrOS tail utility */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define TAIL_BUFSZ 8192

static void tail_fd(int fd, int nlines) {
    /* Read entire file into buffer, then print last N lines */
    char buf[TAIL_BUFSZ];
    int total = 0;
    int r;
    while ((r = read(fd, buf + total, (size_t)(TAIL_BUFSZ - total))) > 0) {
        total += r;
        if (total >= TAIL_BUFSZ) break;
    }

    /* Count newlines from end; skip trailing newline */
    int count = 0;
    int pos = total;
    if (pos > 0 && buf[pos - 1] == '\n') pos--;
    while (pos > 0 && count < nlines) {
        pos--;
        if (buf[pos] == '\n') count++;
    }
    if (pos > 0 || (pos == 0 && buf[0] == '\n')) pos++;

    write(STDOUT_FILENO, buf + pos, (size_t)(total - pos));
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
        tail_fd(STDIN_FILENO, nlines);
    } else {
        for (int i = start; i < argc; i++) {
            if (argc - start > 1) printf("==> %s <==\n", argv[i]);
            int fd = open(argv[i], O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "tail: cannot open '%s'\n", argv[i]);
                continue;
            }
            tail_fd(fd, nlines);
            close(fd);
        }
    }
    return 0;
}
