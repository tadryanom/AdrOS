/* AdrOS wc utility */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

static void wc_fd(int fd, const char* name, int show_l, int show_w, int show_c) {
    char buf[4096];
    int lines = 0, words = 0, chars = 0;
    int in_word = 0;
    int r;

    while ((r = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < r; i++) {
            chars++;
            if (buf[i] == '\n') lines++;
            if (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n') {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                words++;
            }
        }
    }

    if (show_l) printf("%7d", lines);
    if (show_w) printf("%7d", words);
    if (show_c) printf("%7d", chars);
    if (name) printf(" %s", name);
    printf("\n");
}

int main(int argc, char** argv) {
    int show_l = 0, show_w = 0, show_c = 0;
    int start = 1;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1]) {
            const char* f = argv[i] + 1;
            while (*f) {
                if (*f == 'l') show_l = 1;
                else if (*f == 'w') show_w = 1;
                else if (*f == 'c') show_c = 1;
                f++;
            }
            start = i + 1;
        } else break;
    }

    /* Default: show all */
    if (!show_l && !show_w && !show_c) {
        show_l = show_w = show_c = 1;
    }

    if (start >= argc) {
        wc_fd(STDIN_FILENO, NULL, show_l, show_w, show_c);
    } else {
        for (int i = start; i < argc; i++) {
            int fd = open(argv[i], O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "wc: %s: No such file\n", argv[i]);
                continue;
            }
            wc_fd(fd, argv[i], show_l, show_w, show_c);
            close(fd);
        }
    }
    return 0;
}
