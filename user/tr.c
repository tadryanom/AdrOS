/* AdrOS tr utility — translate or delete characters */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int delete_mode = 0;
    int start = 1;

    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        delete_mode = 1;
        start = 2;
    }

    if (delete_mode) {
        if (start >= argc) { fprintf(stderr, "usage: tr -d SET1\n"); return 1; }
        const char* set1 = argv[start];
        char c;
        while (read(STDIN_FILENO, &c, 1) > 0) {
            if (!strchr(set1, c))
                write(STDOUT_FILENO, &c, 1);
        }
    } else {
        if (start + 1 >= argc) { fprintf(stderr, "usage: tr SET1 SET2\n"); return 1; }
        const char* set1 = argv[start];
        const char* set2 = argv[start + 1];
        int len1 = (int)strlen(set1);
        int len2 = (int)strlen(set2);
        char c;
        while (read(STDIN_FILENO, &c, 1) > 0) {
            int found = 0;
            for (int i = 0; i < len1; i++) {
                if (c == set1[i]) {
                    char r = (i < len2) ? set2[i] : set2[len2 - 1];
                    write(STDOUT_FILENO, &r, 1);
                    found = 1;
                    break;
                }
            }
            if (!found) write(STDOUT_FILENO, &c, 1);
        }
    }
    return 0;
}
