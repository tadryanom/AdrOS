/* AdrOS chgrp utility */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: chgrp <group> <file>...\n");
        return 1;
    }

    int group = atoi(argv[1]);
    int rc = 0;

    for (int i = 2; i < argc; i++) {
        if (chown(argv[i], -1, group) < 0) {
            fprintf(stderr, "chgrp: cannot change group of '%s'\n", argv[i]);
            rc = 1;
        }
    }
    return rc;
}
