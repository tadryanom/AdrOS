/* AdrOS awk utility — minimal: print fields, pattern matching, BEGIN/END */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

static char delim = ' ';
static int print_field = -1; /* -1 = whole line */
static char pattern[256] = "";
static int has_pattern = 0;

static void process_line(const char* line) {
    if (has_pattern && !strstr(line, pattern)) return;

    if (print_field < 0) {
        printf("%s\n", line);
        return;
    }

    /* Split into fields */
    char copy[4096];
    strncpy(copy, line, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    int fi = 0;
    char* p = copy;
    while (*p) {
        while (*p && (*p == delim || *p == '\t')) p++;
        if (!*p) break;
        char* start = p;
        while (*p && *p != delim && *p != '\t') p++;
        if (*p) *p++ = '\0';
        if (fi == print_field) {
            printf("%s\n", start);
            return;
        }
        fi++;
    }
    printf("\n");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: awk [-F sep] '{print $N}' [file]\n");
        return 1;
    }

    int argi = 1;
    if (strcmp(argv[argi], "-F") == 0 && argi + 1 < argc) {
        delim = argv[argi + 1][0];
        argi += 2;
    }

    if (argi >= argc) {
        fprintf(stderr, "awk: missing program\n");
        return 1;
    }

    /* Parse simple program: {print $N} or /pattern/{print $N} */
    const char* prog = argv[argi++];

    /* Check for /pattern/ prefix */
    if (prog[0] == '/') {
        const char* end = strchr(prog + 1, '/');
        if (end) {
            int plen = (int)(end - prog - 1);
            if (plen > 0 && plen < (int)sizeof(pattern)) {
                memcpy(pattern, prog + 1, plen);
                pattern[plen] = '\0';
                has_pattern = 1;
            }
            prog = end + 1;
        }
    }

    /* Parse {print $N} */
    const char* pp = strstr(prog, "print");
    if (pp) {
        const char* dollar = strchr(pp, '$');
        if (dollar) {
            int n = atoi(dollar + 1);
            print_field = (n > 0) ? n - 1 : -1;
            if (n == 0) print_field = -1; /* $0 = whole line */
        }
    }

    int fd = STDIN_FILENO;
    if (argi < argc) {
        fd = open(argv[argi], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "awk: %s: No such file or directory\n", argv[argi]);
            return 1;
        }
    }

    char line[4096];
    int li = 0;
    char c;
    while (read(fd, &c, 1) == 1) {
        if (c == '\n') {
            line[li] = '\0';
            process_line(line);
            li = 0;
        } else if (li < (int)sizeof(line) - 1) {
            line[li++] = c;
        }
    }
    if (li > 0) {
        line[li] = '\0';
        process_line(line);
    }

    if (fd != STDIN_FILENO) close(fd);
    return 0;
}
