/* AdrOS mkdir utility */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int pflag = 0;   /* -p: create parent directories */

static int mkdir_p(const char* path) {
    char tmp[256];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return -1;
    strcpy(tmp, path);

    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp);   /* ignore errors — parent may already exist */
            *p = '/';
        }
    }
    return mkdir(tmp);
}

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "mkdir: missing operand\n");
        return 1;
    }

    int rc = 0;
    int start = 1;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1]) {
            const char* f = argv[i] + 1;
            while (*f) {
                if (*f == 'p') pflag = 1;
                else {
                    fprintf(stderr, "mkdir: invalid option -- '%c'\n", *f);
                    return 1;
                }
                f++;
            }
            start = i + 1;
        }
    }

    for (int i = start; i < argc; i++) {
        int r;
        if (pflag) {
            r = mkdir_p(argv[i]);
        } else {
            r = mkdir(argv[i]);
        }
        if (r < 0) {
            fprintf(stderr, "mkdir: cannot create directory '%s'\n", argv[i]);
            rc = 1;
        }
    }
    return rc;
}
