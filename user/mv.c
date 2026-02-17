/* AdrOS mv utility */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: mv <source> <dest>\n");
        return 1;
    }

    /* Try rename first (same filesystem) */
    if (rename(argv[1], argv[2]) == 0)
        return 0;

    /* Fallback: copy + unlink */
    int src = open(argv[1], O_RDONLY);
    if (src < 0) {
        fprintf(stderr, "mv: cannot open '%s'\n", argv[1]);
        return 1;
    }

    int dst = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst < 0) {
        fprintf(stderr, "mv: cannot create '%s'\n", argv[2]);
        close(src);
        return 1;
    }

    char buf[4096];
    int r;
    while ((r = read(src, buf, sizeof(buf))) > 0) {
        if (write(dst, buf, (size_t)r) != r) {
            fprintf(stderr, "mv: write error\n");
            close(src); close(dst);
            return 1;
        }
    }

    close(src);
    close(dst);
    unlink(argv[1]);
    return 0;
}
