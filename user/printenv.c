/* AdrOS printenv utility — print environment variables */
#include <stdio.h>
#include <string.h>

extern char** __environ;

int main(int argc, char** argv) {
    if (!__environ) return 1;
    if (argc <= 1) {
        for (int i = 0; __environ[i]; i++)
            printf("%s\n", __environ[i]);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        int found = 0;
        int nlen = (int)strlen(argv[i]);
        for (int j = 0; __environ[j]; j++) {
            if (strncmp(__environ[j], argv[i], (size_t)nlen) == 0 && __environ[j][nlen] == '=') {
                printf("%s\n", __environ[j] + nlen + 1);
                found = 1;
                break;
            }
        }
        if (!found) return 1;
    }
    return 0;
}
