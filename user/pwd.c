/* AdrOS pwd utility — print working directory */
#include <stdio.h>
#include <unistd.h>

int main(void) {
    char buf[256];
    if (getcwd(buf, sizeof(buf)) >= 0)
        printf("%s\n", buf);
    else {
        fprintf(stderr, "pwd: error\n");
        return 1;
    }
    return 0;
}
