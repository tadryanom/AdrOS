/* AdrOS basename utility — strip directory from filename */
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "usage: basename PATH [SUFFIX]\n");
        return 1;
    }
    char* p = argv[1];
    /* Remove trailing slashes */
    int len = (int)strlen(p);
    while (len > 1 && p[len - 1] == '/') p[--len] = '\0';
    /* Find last slash */
    char* base = p;
    for (char* s = p; *s; s++) {
        if (*s == '/' && *(s + 1)) base = s + 1;
    }
    /* Strip suffix if provided */
    if (argc > 2) {
        int blen = (int)strlen(base);
        int slen = (int)strlen(argv[2]);
        if (blen > slen && strcmp(base + blen - slen, argv[2]) == 0)
            base[blen - slen] = '\0';
    }
    printf("%s\n", base);
    return 0;
}
