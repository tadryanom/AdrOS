/* AdrOS ln utility */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int sflag = 0;
    int start = 1;

    if (argc > 1 && strcmp(argv[1], "-s") == 0) {
        sflag = 1;
        start = 2;
    }

    if (argc - start < 2) {
        fprintf(stderr, "Usage: ln [-s] <target> <linkname>\n");
        return 1;
    }

    int r;
    if (sflag) {
        r = symlink(argv[start], argv[start + 1]);
    } else {
        r = link(argv[start], argv[start + 1]);
    }

    if (r < 0) {
        fprintf(stderr, "ln: failed to create %slink '%s' -> '%s'\n",
                sflag ? "symbolic " : "", argv[start + 1], argv[start]);
        return 1;
    }
    return 0;
}
