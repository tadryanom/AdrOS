/* AdrOS touch utility */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: touch <file>...\n");
        return 1;
    }

    int rc = 0;
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_WRONLY | O_CREAT);
        if (fd < 0) {
            fprintf(stderr, "touch: cannot touch '%s'\n", argv[i]);
            rc = 1;
        } else {
            close(fd);
        }
    }
    return rc;
}
