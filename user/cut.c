/* AdrOS cut utility */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define LINE_MAX 1024

static char delim = '\t';
static int fields[32];
static int nfields = 0;

static void parse_fields(const char* spec) {
    const char* p = spec;
    while (*p && nfields < 32) {
        fields[nfields++] = atoi(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
}

static void cut_line(char* line) {
    if (nfields == 0) {
        printf("%s\n", line);
        return;
    }

    /* Split line into fields */
    char* flds[64];
    int nf = 0;
    char* p = line;
    flds[nf++] = p;
    while (*p && nf < 64) {
        if (*p == delim) {
            *p = '\0';
            flds[nf++] = p + 1;
        }
        p++;
    }

    /* Print requested fields */
    int first = 1;
    for (int i = 0; i < nfields; i++) {
        int idx = fields[i] - 1;  /* 1-based */
        if (idx >= 0 && idx < nf) {
            if (!first) write(STDOUT_FILENO, &delim, 1);
            printf("%s", flds[idx]);
            first = 0;
        }
    }
    printf("\n");
}

static void cut_fd(int fd) {
    char line[LINE_MAX];
    int pos = 0;
    char c;

    while (read(fd, &c, 1) > 0) {
        if (c == '\n') {
            line[pos] = '\0';
            cut_line(line);
            pos = 0;
        } else if (pos < LINE_MAX - 1) {
            line[pos++] = c;
        }
    }
    if (pos > 0) {
        line[pos] = '\0';
        cut_line(line);
    }
}

int main(int argc, char** argv) {
    int start = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            delim = argv[++i][0];
            start = i + 1;
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            parse_fields(argv[++i]);
            start = i + 1;
        } else if (argv[i][0] != '-') {
            break;
        }
    }

    if (start >= argc) {
        cut_fd(STDIN_FILENO);
    } else {
        for (int i = start; i < argc; i++) {
            int fd = open(argv[i], O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "cut: cannot open '%s'\n", argv[i]);
                continue;
            }
            cut_fd(fd);
            close(fd);
        }
    }
    return 0;
}
