/* AdrOS cp utility */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: cp <source> <dest>\n");
        return 1;
    }

    int src = open(argv[1], O_RDONLY);
    if (src < 0) {
        fprintf(stderr, "cp: cannot open '%s'\n", argv[1]);
        return 1;
    }

    int dst = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC);
    if (dst < 0) {
        fprintf(stderr, "cp: cannot create '%s'\n", argv[2]);
        close(src);
        return 1;
    }

    char buf[4096];
    int r;
    while ((r = read(src, buf, sizeof(buf))) > 0) {
        int w = write(dst, buf, (size_t)r);
        if (w != r) {
            fprintf(stderr, "cp: write error\n");
            close(src);
            close(dst);
            return 1;
        }
    }

    close(src);
    close(dst);
    return 0;
}
