/* AdrOS sort utility */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_LINES 1024
#define LINE_BUF  65536

static char linebuf[LINE_BUF];
static char* lines[MAX_LINES];
static int nlines = 0;

static int rflag = 0;  /* -r: reverse */
static int nflag = 0;  /* -n: numeric */

static int cmp(const void* a, const void* b) {
    const char* sa = *(const char**)a;
    const char* sb = *(const char**)b;
    int r;
    if (nflag) {
        r = atoi(sa) - atoi(sb);
    } else {
        r = strcmp(sa, sb);
    }
    return rflag ? -r : r;
}

static void read_lines(int fd) {
    int total = 0;
    int r;
    while ((r = read(fd, linebuf + total, (size_t)(LINE_BUF - total - 1))) > 0) {
        total += r;
        if (total >= LINE_BUF - 1) break;
    }
    linebuf[total] = '\0';

    /* Split into lines */
    char* p = linebuf;
    while (*p && nlines < MAX_LINES) {
        lines[nlines++] = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') *p++ = '\0';
    }
}

int main(int argc, char** argv) {
    int start = 1;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            const char* f = argv[i] + 1;
            while (*f) {
                if (*f == 'r') rflag = 1;
                else if (*f == 'n') nflag = 1;
                f++;
            }
            start = i + 1;
        } else break;
    }

    if (start >= argc) {
        read_lines(STDIN_FILENO);
    } else {
        for (int i = start; i < argc; i++) {
            int fd = open(argv[i], O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "sort: cannot open '%s'\n", argv[i]);
                return 1;
            }
            read_lines(fd);
            close(fd);
        }
    }

    /* Simple insertion sort (no qsort in ulibc yet) */
    for (int i = 1; i < nlines; i++) {
        char* key = lines[i];
        int j = i - 1;
        while (j >= 0 && cmp(&lines[j], &key) > 0) {
            lines[j + 1] = lines[j];
            j--;
        }
        lines[j + 1] = key;
    }

    for (int i = 0; i < nlines; i++)
        printf("%s\n", lines[i]);

    return 0;
}
