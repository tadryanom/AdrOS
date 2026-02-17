/* AdrOS rm utility */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int rflag = 0;   /* -r: recursive */
static int fflag = 0;   /* -f: force (no errors) */
static int dflag = 0;   /* -d: remove empty directories */

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "rm: missing operand\n");
        return 1;
    }

    int start = 1;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1]) {
            const char* f = argv[i] + 1;
            while (*f) {
                if (*f == 'r' || *f == 'R') rflag = 1;
                else if (*f == 'f') fflag = 1;
                else if (*f == 'd') dflag = 1;
                else {
                    fprintf(stderr, "rm: invalid option -- '%c'\n", *f);
                    return 1;
                }
                f++;
            }
            start = i + 1;
        } else {
            break;
        }
    }

    int rc = 0;
    for (int i = start; i < argc; i++) {
        int r = unlink(argv[i]);
        if (r < 0 && (rflag || dflag)) {
            r = rmdir(argv[i]);
        }
        if (r < 0 && !fflag) {
            fprintf(stderr, "rm: cannot remove '%s'\n", argv[i]);
            rc = 1;
        }
    }
    return rc;
}
