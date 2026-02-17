/* AdrOS stat utility — display file status */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "usage: stat FILE...\n");
        return 1;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], (void*)&st) < 0) {
            fprintf(stderr, "stat: cannot stat '%s'\n", argv[i]);
            rc = 1;
            continue;
        }
        printf("  File: %s\n", argv[i]);
        printf("  Size: %u\tInode: %u\n", (unsigned)st.st_size, (unsigned)st.st_ino);
        printf("  Mode: %o\tUid: %u\tGid: %u\n", (unsigned)st.st_mode, (unsigned)st.st_uid, (unsigned)st.st_gid);
    }
    return rc;
}
