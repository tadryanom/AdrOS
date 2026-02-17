/* AdrOS df utility — display filesystem disk space usage */
#include <stdio.h>

int main(void) {
    printf("Filesystem     Size  Used  Avail  Use%%  Mounted on\n");
    printf("overlayfs         -     -      -     -  /\n");
    printf("devfs             -     -      -     -  /dev\n");
    printf("procfs            -     -      -     -  /proc\n");
    return 0;
}
