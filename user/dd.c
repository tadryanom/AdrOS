/* AdrOS dd utility — convert and copy a file */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

static int parse_size(const char* s) {
    int v = atoi(s);
    int len = (int)strlen(s);
    if (len > 0) {
        char suf = s[len - 1];
        if (suf == 'k' || suf == 'K') v *= 1024;
        else if (suf == 'm' || suf == 'M') v *= 1024 * 1024;
    }
    return v;
}

int main(int argc, char** argv) {
    const char* inf = NULL;
    const char* outf = NULL;
    int bs = 512;
    int count = -1;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "if=", 3) == 0) inf = argv[i] + 3;
        else if (strncmp(argv[i], "of=", 3) == 0) outf = argv[i] + 3;
        else if (strncmp(argv[i], "bs=", 3) == 0) bs = parse_size(argv[i] + 3);
        else if (strncmp(argv[i], "count=", 6) == 0) count = atoi(argv[i] + 6);
    }

    int ifd = STDIN_FILENO;
    int ofd = STDOUT_FILENO;

    if (inf) {
        ifd = open(inf, O_RDONLY);
        if (ifd < 0) { fprintf(stderr, "dd: cannot open '%s'\n", inf); return 1; }
    }
    if (outf) {
        ofd = open(outf, O_WRONLY | O_CREAT | O_TRUNC);
        if (ofd < 0) { fprintf(stderr, "dd: cannot open '%s'\n", outf); return 1; }
    }

    if (bs > 4096) bs = 4096;
    char buf[4096];
    int blocks = 0, partial = 0, total = 0;

    while (count < 0 || blocks + partial < count) {
        int n = read(ifd, buf, (size_t)bs);
        if (n <= 0) break;
        write(ofd, buf, (size_t)n);
        total += n;
        if (n == bs) blocks++;
        else partial++;
    }

    fprintf(stderr, "%d+%d records in\n%d+%d records out\n%d bytes copied\n",
            blocks, partial, blocks, partial, total);

    if (inf) close(ifd);
    if (outf) close(ofd);
    return 0;
}
