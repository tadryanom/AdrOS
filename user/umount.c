/* AdrOS umount utility — stub (no SYS_UMOUNT syscall yet) */
#include <stdio.h>

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "umount: missing operand\n");
        return 1;
    }
    fprintf(stderr, "umount: %s: operation not supported\n", argv[1]);
    return 1;
}
