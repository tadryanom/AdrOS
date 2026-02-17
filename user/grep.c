/* AdrOS grep utility — search for pattern in files */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static int match_simple(const char* text, const char* pat) {
    /* Simple substring match (no regex) */
    return strstr(text, pat) != NULL;
}

static int grep_fd(int fd, const char* pattern, const char* fname, int show_name, int invert, int count_only, int line_num) {
    char buf[4096];
    int pos = 0, n, matches = 0, lnum = 0;
    while ((n = read(fd, buf + pos, (size_t)(sizeof(buf) - 1 - pos))) > 0) {
        pos += n;
        buf[pos] = '\0';
        char* start = buf;
        char* nl;
        while ((nl = strchr(start, '\n')) != NULL) {
            *nl = '\0';
            lnum++;
            int m = match_simple(start, pattern);
            if (invert) m = !m;
            if (m) {
                matches++;
                if (!count_only) {
                    if (show_name) printf("%s:", fname);
                    if (line_num) printf("%d:", lnum);
                    printf("%s\n", start);
                }
            }
            start = nl + 1;
        }
        int rem = (int)(buf + pos - start);
        if (rem > 0) memmove(buf, start, (size_t)rem);
        pos = rem;
    }
    if (pos > 0) {
        buf[pos] = '\0';
        lnum++;
        int m = match_simple(buf, pattern);
        if (invert) m = !m;
        if (m) {
            matches++;
            if (!count_only) {
                if (show_name) printf("%s:", fname);
                if (line_num) printf("%d:", lnum);
                printf("%s\n", buf);
            }
        }
    }
    if (count_only) printf("%s%s%d\n", show_name ? fname : "", show_name ? ":" : "", matches);
    return matches > 0 ? 0 : 1;
}

int main(int argc, char** argv) {
    int invert = 0, count_only = 0, line_num = 0;
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        for (int j = 1; argv[i][j]; j++) {
            if (argv[i][j] == 'v') invert = 1;
            else if (argv[i][j] == 'c') count_only = 1;
            else if (argv[i][j] == 'n') line_num = 1;
        }
        i++;
    }
    if (i >= argc) { fprintf(stderr, "usage: grep [-vcn] PATTERN [FILE...]\n"); return 2; }
    const char* pattern = argv[i++];
    if (i >= argc) return grep_fd(STDIN_FILENO, pattern, "(stdin)", 0, invert, count_only, line_num);
    int rc = 1, nfiles = argc - i;
    for (; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) { fprintf(stderr, "grep: %s: No such file or directory\n", argv[i]); continue; }
        if (grep_fd(fd, pattern, argv[i], nfiles > 1, invert, count_only, line_num) == 0) rc = 0;
        close(fd);
    }
    return rc;
}
