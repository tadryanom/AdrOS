/* AdrOS chmod utility */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: chmod <mode> <file>...\n");
        return 1;
    }

    int mode = (int)strtol(argv[1], NULL, 8);
    int rc = 0;

    for (int i = 2; i < argc; i++) {
        if (chmod(argv[i], mode) < 0) {
            fprintf(stderr, "chmod: cannot change mode of '%s'\n", argv[i]);
            rc = 1;
        }
    }
    return rc;
}
