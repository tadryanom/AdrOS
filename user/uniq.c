/* AdrOS uniq utility */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define LINE_MAX 1024

static int cflag = 0;  /* -c: prefix lines with count */
static int dflag = 0;  /* -d: only print duplicates */

static int readline(int fd, char* buf, int max) {
    int n = 0;
    char c;
    while (n < max - 1) {
        int r = read(fd, &c, 1);
        if (r <= 0) break;
        if (c == '\n') break;
        buf[n++] = c;
    }
    buf[n] = '\0';
    return n > 0 ? n : (n == 0 ? 0 : -1);
}

int main(int argc, char** argv) {
    int start = 1;
    int fd = STDIN_FILENO;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1]) {
            const char* f = argv[i] + 1;
            while (*f) {
                if (*f == 'c') cflag = 1;
                else if (*f == 'd') dflag = 1;
                f++;
            }
            start = i + 1;
        } else break;
    }

    if (start < argc) {
        fd = open(argv[start], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "uniq: cannot open '%s'\n", argv[start]);
            return 1;
        }
    }

    char prev[LINE_MAX] = {0};
    char cur[LINE_MAX];
    int count = 0;
    int first = 1;

    while (1) {
        int r = readline(fd, cur, LINE_MAX);
        if (r < 0) break;

        if (first || strcmp(cur, prev) != 0) {
            if (!first) {
                if (!dflag || count > 1) {
                    if (cflag) printf("%7d %s\n", count, prev);
                    else printf("%s\n", prev);
                }
            }
            strcpy(prev, cur);
            count = 1;
            first = 0;
        } else {
            count++;
        }

        if (r == 0) break;
    }

    /* Print last line */
    if (!first) {
        if (!dflag || count > 1) {
            if (cflag) printf("%7d %s\n", count, prev);
            else printf("%s\n", prev);
        }
    }

    if (fd != STDIN_FILENO) close(fd);
    return 0;
}
