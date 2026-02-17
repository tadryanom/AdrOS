/* AdrOS echo utility — POSIX-compatible */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int nflag = 0;      /* -n: no trailing newline */
    int eflag = 0;      /* -e: interpret escape sequences */
    int i = 1;

    /* Parse flags */
    while (i < argc && argv[i][0] == '-' && argv[i][1] != '\0') {
        const char* f = argv[i] + 1;
        int valid = 1;
        int n = 0, e = 0;
        while (*f) {
            if (*f == 'n') n = 1;
            else if (*f == 'e') e = 1;
            else if (*f == 'E') { /* no escapes — default */ }
            else { valid = 0; break; }
            f++;
        }
        if (!valid) break;
        if (n) nflag = 1;
        if (e) eflag = 1;
        i++;
    }

    for (; i < argc; i++) {
        if (i > 1 && (i > 1 || nflag || eflag))
            write(STDOUT_FILENO, " ", 1);

        const char* s = argv[i];
        if (eflag) {
            while (*s) {
                if (*s == '\\' && s[1]) {
                    s++;
                    char c = *s;
                    switch (c) {
                    case 'n': write(STDOUT_FILENO, "\n", 1); break;
                    case 't': write(STDOUT_FILENO, "\t", 1); break;
                    case '\\': write(STDOUT_FILENO, "\\", 1); break;
                    case 'r': write(STDOUT_FILENO, "\r", 1); break;
                    case 'a': write(STDOUT_FILENO, "\a", 1); break;
                    default: write(STDOUT_FILENO, "\\", 1);
                             write(STDOUT_FILENO, &c, 1); break;
                    }
                } else {
                    write(STDOUT_FILENO, s, 1);
                }
                s++;
            }
        } else {
            write(STDOUT_FILENO, s, strlen(s));
        }
    }

    if (!nflag)
        write(STDOUT_FILENO, "\n", 1);

    return 0;
}
